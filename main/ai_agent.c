#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/base64.h"
#include "cJSON.h"
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_HTTP_OUTPUT_BUFFER 2048  // Reduced from 4096 to save memory
#define BASE64_AUTH_LEN        256
#define AGORA_API_URL          "https://api.agora.io/api/conversational-ai-agent/v2/projects"

static char http_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
static int http_response_len = 0;

/* Forward declarations */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static int _stop_agent_by_id(const char *agent_id);
void ai_agent_start(void);

/* Retry task to avoid stack overflow from recursive calls */
static void _agent_start_retry_task(void *pvParameters)
{
    int delay_ms = (int)pvParameters;

    if (delay_ms > 0) {
        printf("Waiting %d ms before retry...\n", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    printf("========================================\n");
    printf("Retrying agent start...\n");
    printf("========================================\n");

    ai_agent_start();

    // Task done, delete itself
    vTaskDelete(NULL);
}

/* Generate Base64 encoded credentials for Basic Auth */
static void _generate_basic_auth(char *output, size_t output_len)
{
    char credentials[128];
    snprintf(credentials, sizeof(credentials), "%s:%s", AGORA_API_KEY, AGORA_API_SECRET);

    size_t olen = 0;
    mbedtls_base64_encode((unsigned char *)output, output_len, &olen,
                          (const unsigned char *)credentials, strlen(credentials));
}

/* Parse response from /join endpoint to extract agent_id */
/* Returns: 0 = success, -1 = error, -2 = task conflict */
static int _parse_join_response(const char *resp_data, int data_len)
{
    if (data_len == 0) {
        printf("Empty response data\n");
        return -1;
    }

    cJSON *root = cJSON_Parse(resp_data);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("JSON parse error: %s\n", error_ptr);
        }
        return -1;
    }

    // Check for agent_id in response
    cJSON *agent_id = cJSON_GetObjectItemCaseSensitive(root, "agent_id");
    if (cJSON_IsString(agent_id) && (agent_id->valuestring != NULL)) {
        printf("✓ Agent ID: %s\n", agent_id->valuestring);
        snprintf(g_app.agent_id, AGENT_ID_LEN, "%s", agent_id->valuestring);
        g_app.b_ai_agent_joined = true;
        printf("✓ Flag updated: b_ai_agent_joined=true, agent_id='%s'\n", g_app.agent_id);
        cJSON_Delete(root);
        return 0;
    }

    // If no agent_id, check for error
    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(root, "reason");
    cJSON *detail = cJSON_GetObjectItemCaseSensitive(root, "detail");

    if (cJSON_IsNumber(code)) {
        printf("Error code: %d\n", code->valueint);
    }
    if (cJSON_IsString(message) && (message->valuestring != NULL)) {
        printf("Error message: %s\n", message->valuestring);
    }
    if (cJSON_IsString(reason) && (reason->valuestring != NULL)) {
        printf("Error reason: %s\n", reason->valuestring);
    }
    if (cJSON_IsString(detail) && (detail->valuestring != NULL)) {
        printf("Error detail: %s\n", detail->valuestring);
    }

    // Check for TaskConflict
    bool is_conflict = false;
    if (cJSON_IsString(reason) && (reason->valuestring != NULL)) {
        if (strcmp(reason->valuestring, "TaskConflict") == 0) {
            is_conflict = true;
        }
    }
    if (!is_conflict && cJSON_IsString(detail) && (detail->valuestring != NULL)) {
        if (strstr(detail->valuestring, "conflict task") != NULL) {
            is_conflict = true;
        }
    }

    cJSON_Delete(root);
    return is_conflict ? -2 : -1;
}

/* Get running agent ID from list of active agents */
/* Returns: 0 = success, -1 = error */
static int _get_running_agent_id(char *agent_id_out, size_t agent_id_len)
{
    if (!agent_id_out || agent_id_len == 0) {
        return -1;
    }

    printf("Querying running agents...\n");

    // Generate authorization header
    char auth_header[BASE64_AUTH_LEN];
    _generate_basic_auth(auth_header, sizeof(auth_header));

    char auth_value[BASE64_AUTH_LEN + 10];
    snprintf(auth_value, sizeof(auth_value), "Basic %s", auth_header);

    // Build URL - state=2 means running agents
    char url[256];
    snprintf(url, sizeof(url), "%s/%s/agents?state=2&limit=20",
             AGORA_API_URL, AGORA_APP_ID);

    printf("Request URL: %s\n", url);

    // Initialize HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", auth_value);

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        printf("HTTP GET request failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    printf("HTTP Status = %d\n", status_code);

    if (status_code != 200) {
        printf("Failed to get running agents, status: %d\n", status_code);
        esp_http_client_cleanup(client);
        return -1;
    }

    // Parse response
    cJSON *root = cJSON_Parse(http_response_buffer);
    if (root == NULL) {
        printf("Failed to parse agents list JSON\n");
        esp_http_client_cleanup(client);
        return -1;
    }

    // Get data object first
    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        printf("No data object in response\n");
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return -1;
    }

    // Get list array from data object
    cJSON *agents = cJSON_GetObjectItemCaseSensitive(data, "list");
    if (!cJSON_IsArray(agents)) {
        printf("No list array in data object\n");
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return -1;
    }

    int array_size = cJSON_GetArraySize(agents);
    printf("Found %d running agent(s)\n", array_size);

    if (array_size == 0) {
        printf("No running agents found\n");
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return -1;
    }

    // Get first agent's ID
    cJSON *first_agent = cJSON_GetArrayItem(agents, 0);
    cJSON *agent_id_json = cJSON_GetObjectItemCaseSensitive(first_agent, "agent_id");

    if (cJSON_IsString(agent_id_json) && (agent_id_json->valuestring != NULL)) {
        snprintf(agent_id_out, agent_id_len, "%s", agent_id_json->valuestring);
        printf("✓ Found running agent ID: %s\n", agent_id_out);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        return 0;
    }

    printf("Failed to extract agent_id from response\n");
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
    return -1;
}

/* Parse response from /leave endpoint */
static int _parse_leave_response(const char *resp_data, int data_len)
{
    if (data_len == 0) {
        printf("Empty response data\n");
        return -1;
    }

    cJSON *root = cJSON_Parse(resp_data);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("JSON parse error: %s\n", error_ptr);
        }
        return -1;
    }

    // Check for success
    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (cJSON_IsNumber(code) && code->valueint == 0) {
        printf("✓ Agent left successfully\n");
        g_app.b_ai_agent_joined = false;
        memset(g_app.agent_id, 0, AGENT_ID_LEN);
        printf("✓ Flag updated: b_ai_agent_joined=false, agent_id cleared\n");
        cJSON_Delete(root);
        return 0;
    }

    // Check for error
    cJSON *message = cJSON_GetObjectItemCaseSensitive(root, "message");
    if (cJSON_IsString(message) && (message->valuestring != NULL)) {
        printf("Error message: %s\n", message->valuestring);
    }

    cJSON_Delete(root);
    return -1;
}

/* HTTP event handler */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP_EVENT_ON_CONNECTED\n");
            http_response_len = 0;
            memset(http_response_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("HTTP_EVENT_HEADER_SENT\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            printf("HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - http_response_len - 1));
                if (copy_len > 0) {
                    memcpy(http_response_buffer + http_response_len, evt->data, copy_len);
                    http_response_len += copy_len;
                    http_response_buffer[http_response_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            printf("HTTP_EVENT_ON_FINISH\n");
            printf("Response: %s\n", http_response_buffer);
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("HTTP_EVENT_DISCONNECTED\n");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Build JSON for /join request */
static char *_build_join_json(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    // Add name
    cJSON_AddStringToObject(root, "name", CONVO_AGENT_NAME);

    // Create properties object
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "properties", properties);

    // Add channel and token
    cJSON_AddStringToObject(properties, "channel", CONVO_CHANNEL_NAME);
    cJSON_AddStringToObject(properties, "token", CONVO_RTC_TOKEN);

    // Add UIDs (as strings, not numbers)
    char agent_uid_str[16];
    char remote_uid_str[16];
    snprintf(agent_uid_str, sizeof(agent_uid_str), "%d", CONVO_AGENT_RTC_UID);
    snprintf(remote_uid_str, sizeof(remote_uid_str), "%d", CONVO_REMOTE_RTC_UID);

    cJSON_AddStringToObject(properties, "agent_rtc_uid", agent_uid_str);
    cJSON *remote_uids = cJSON_CreateArray();
    cJSON_AddItemToArray(remote_uids, cJSON_CreateString(remote_uid_str));
    cJSON_AddItemToObject(properties, "remote_rtc_uids", remote_uids);

    // Add parameters
    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddStringToObject(parameters, "output_audio_codec", "PCMU");
    cJSON_AddItemToObject(properties, "parameters", parameters);

    // Add idle timeout
    cJSON_AddNumberToObject(properties, "idle_timeout", CONVO_IDLE_TIMEOUT);

    // Add advanced features
    cJSON *advanced_features = cJSON_CreateObject();
    cJSON_AddBoolToObject(advanced_features, "enable_aivad", CONVO_ENABLE_AIVAD);
    cJSON_AddItemToObject(properties, "advanced_features", advanced_features);

    // Add LLM configuration
    cJSON *llm = cJSON_CreateObject();
    cJSON_AddStringToObject(llm, "url", LLM_URL);
    cJSON_AddStringToObject(llm, "api_key", LLM_API_KEY);

    // System messages array
    cJSON *system_messages = cJSON_CreateArray();
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", LLM_SYSTEM_MESSAGE);
    cJSON_AddItemToArray(system_messages, sys_msg);
    cJSON_AddItemToObject(llm, "system_messages", system_messages);

    cJSON_AddNumberToObject(llm, "max_history", LLM_MAX_HISTORY);
    cJSON_AddStringToObject(llm, "greeting_message", LLM_GREETING_MESSAGE);
    cJSON_AddStringToObject(llm, "failure_message", LLM_FAILURE_MESSAGE);

    // LLM params
    cJSON *llm_params = cJSON_CreateObject();
    cJSON_AddStringToObject(llm_params, "model", LLM_MODEL);
    cJSON_AddItemToObject(llm, "params", llm_params);

    cJSON_AddItemToObject(properties, "llm", llm);

    // Add TTS configuration
    cJSON *tts = cJSON_CreateObject();

#ifdef USE_TTS_CARTESIA
    // Cartesia TTS configuration
    cJSON_AddStringToObject(tts, "vendor", TTS_CARTESIA_VENDOR);

    cJSON *tts_params = cJSON_CreateObject();
    cJSON_AddStringToObject(tts_params, "api_key", TTS_CARTESIA_API_KEY);
    cJSON_AddStringToObject(tts_params, "model_id", TTS_CARTESIA_MODEL_ID);

    // Add voice object
    cJSON *voice = cJSON_CreateObject();
    cJSON_AddStringToObject(voice, "mode", TTS_CARTESIA_VOICE_MODE);
    cJSON_AddStringToObject(voice, "id", TTS_CARTESIA_VOICE_ID);
    cJSON_AddItemToObject(tts_params, "voice", voice);

    // Add output_format object
    cJSON *output_format = cJSON_CreateObject();
    cJSON_AddStringToObject(output_format, "container", TTS_CARTESIA_CONTAINER);
    cJSON_AddNumberToObject(output_format, "sample_rate", TTS_CARTESIA_SAMPLE_RATE);
    cJSON_AddItemToObject(tts_params, "output_format", output_format);

    cJSON_AddStringToObject(tts_params, "language", TTS_CARTESIA_LANGUAGE);
    cJSON_AddItemToObject(tts, "params", tts_params);
#else
    // Microsoft TTS configuration
    cJSON_AddStringToObject(tts, "vendor", TTS_MICROSOFT_VENDOR);

    cJSON *tts_params = cJSON_CreateObject();
    cJSON_AddStringToObject(tts_params, "key", TTS_MICROSOFT_API_KEY);
    cJSON_AddStringToObject(tts_params, "region", TTS_MICROSOFT_REGION);
    cJSON_AddStringToObject(tts_params, "voice_name", TTS_MICROSOFT_VOICE_NAME);
    cJSON_AddItemToObject(tts, "params", tts_params);
#endif

    cJSON_AddItemToObject(properties, "tts", tts);

    // Add ASR configuration
    cJSON *asr = cJSON_CreateObject();
    cJSON_AddStringToObject(asr, "language", ASR_LANGUAGE);
    cJSON_AddItemToObject(properties, "asr", asr);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);

    return json_string;
}

/* Start conversational AI agent */
void ai_agent_start(void)
{
    // Check if agent is already started
    if (g_app.b_ai_agent_joined) {
        printf("AI Agent already running\n");
        return;
    }

    printf("Starting conversational AI agent...\n");

    // Generate authorization header
    char auth_header[BASE64_AUTH_LEN];
    _generate_basic_auth(auth_header, sizeof(auth_header));

    char auth_value[BASE64_AUTH_LEN + 10];
    snprintf(auth_value, sizeof(auth_value), "Basic %s", auth_header);

    // Build URL
    char url[256];
    snprintf(url, sizeof(url), "%s/%s/join", AGORA_API_URL, AGORA_APP_ID);

    // Build JSON request
    char *json_data = _build_join_json();
    if (json_data == NULL) {
        printf("Failed to build JSON request\n");
        return;
    }

    printf("Request URL: %s\n", url);
    printf("Request JSON: %s\n", json_data);

    // Initialize HTTP client with SSL configuration
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Use certificate bundle for HTTPS
        .skip_cert_common_name_check = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_value);

    // Send request
    esp_err_t err = esp_http_client_open(client, strlen(json_data));
    if (err != ESP_OK) {
        printf("Failed to open HTTP connection: %s\n", esp_err_to_name(err));
        free(json_data);
        esp_http_client_cleanup(client);
        return;
    }

    int wlen = esp_http_client_write(client, json_data, strlen(json_data));
    if (wlen < 0) {
        printf("Failed to write HTTP request\n");
        free(json_data);
        esp_http_client_cleanup(client);
        return;
    }

    free(json_data);

    // Perform request
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        printf("HTTP Status = %d, content_length = %lld\n",
               status_code, esp_http_client_get_content_length(client));

        // Check for conflict (409)
        if (status_code == 409) {
            printf("========================================\n");
            printf("⚠️  CONFLICT: Agent already exists in channel\n");
            printf("Attempting to resolve conflict...\n");
            printf("========================================\n");

            esp_http_client_cleanup(client);

            // Step 1: Get running agent ID
            char running_agent_id[AGENT_ID_LEN] = {0};
            int query_result = _get_running_agent_id(running_agent_id, sizeof(running_agent_id));

            if (query_result == 0) {
                // Found a running agent - need to stop it
                printf("Found conflicting agent: %s\n", running_agent_id);

                // Step 2: Stop the running agent
                if (_stop_agent_by_id(running_agent_id) == 0) {
                    printf("✓ Conflicting agent stopped\n");

                    // Step 3: Spawn retry task with 2 second delay
                    xTaskCreate(_agent_start_retry_task, "agent_retry", 8192, (void*)2000, 5, NULL);
                    return;
                } else {
                    printf("✗ Failed to stop conflicting agent\n");
                    g_app.b_ai_agent_joined = false;
                    memset(g_app.agent_id, 0, AGENT_ID_LEN);
                    return;
                }
            } else {
                // No running agents found - the conflict is stale
                printf("✓ No running agents found (conflict is stale)\n");

                // Spawn retry task with 1 second delay
                xTaskCreate(_agent_start_retry_task, "agent_retry", 8192, (void*)1000, 5, NULL);
                return;
            }
        }

        // Parse response for success or other errors
        if (status_code == 200) {
            _parse_join_response(http_response_buffer, http_response_len);
        } else {
            // Non-200, non-409 status: start failed
            printf("✗ Start request failed with status %d\n", status_code);
            printf("✗ Agent was not created\n");
            // Ensure state is clean
            g_app.b_ai_agent_joined = false;
            memset(g_app.agent_id, 0, AGENT_ID_LEN);
        }
    } else {
        printf("HTTP request failed: %s\n", esp_err_to_name(err));
        printf("✗ Agent was not created\n");
        // Ensure state is clean
        g_app.b_ai_agent_joined = false;
        memset(g_app.agent_id, 0, AGENT_ID_LEN);
    }

    esp_http_client_cleanup(client);
}

/* Stop a specific agent by ID */
static int _stop_agent_by_id(const char *agent_id)
{
    if (!agent_id || strlen(agent_id) == 0) {
        printf("✗ ERROR: Invalid agent_id\n");
        return -1;
    }

    printf("Stopping agent ID: %s\n", agent_id);

    // Generate authorization header
    char auth_header[BASE64_AUTH_LEN];
    _generate_basic_auth(auth_header, sizeof(auth_header));

    char auth_value[BASE64_AUTH_LEN + 10];
    snprintf(auth_value, sizeof(auth_value), "Basic %s", auth_header);

    // Build URL with agent_id
    char url[256];
    snprintf(url, sizeof(url), "%s/%s/agents/%s/leave",
             AGORA_API_URL, AGORA_APP_ID, agent_id);

    printf("Request URL: %s\n", url);

    // Initialize HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth_value);

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        printf("HTTP request failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    printf("HTTP Status = %d\n", status_code);

    esp_http_client_cleanup(client);

    if (status_code == 200) {
        printf("✓ Agent %s stopped successfully\n", agent_id);
        return 0;
    } else {
        printf("⚠ Stop request returned status %d\n", status_code);
        return -1;
    }
}

/* Stop conversational AI agent */
void ai_agent_stop(void)
{
    printf("========================================\n");
    printf("Stopping conversational AI agent...\n");
    printf("Current state: b_ai_agent_joined=%d, agent_id='%s'\n",
           g_app.b_ai_agent_joined, g_app.agent_id);
    printf("========================================\n");

    if (strlen(g_app.agent_id) == 0) {
        printf("✗ ERROR: No active agent to stop (agent_id is empty)\n");
        printf("This means start request may have failed or not been called\n");
        // Clear flag anyway to ensure clean state
        g_app.b_ai_agent_joined = false;
        return;
    }

    // Generate authorization header
    char auth_header[BASE64_AUTH_LEN];
    _generate_basic_auth(auth_header, sizeof(auth_header));

    char auth_value[BASE64_AUTH_LEN + 10];
    snprintf(auth_value, sizeof(auth_value), "Basic %s", auth_header);

    // Build URL with agent_id
    char url[256];
    snprintf(url, sizeof(url), "%s/%s/agents/%s/leave",
             AGORA_API_URL, AGORA_APP_ID, g_app.agent_id);

    printf("Request URL: %s\n", url);

    // Initialize HTTP client with SSL configuration
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Use certificate bundle for HTTPS
        .skip_cert_common_name_check = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth_value);

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        printf("HTTP Status = %d, content_length = %lld\n",
               status_code, esp_http_client_get_content_length(client));

        // Parse response
        if (status_code == 200) {
            _parse_leave_response(http_response_buffer, http_response_len);
        } else {
            // Non-200 status: agent may not exist, may have timed out, etc.
            // Clear state anyway to prevent getting stuck
            printf("⚠ Stop request failed with status %d, clearing state anyway\n", status_code);
            g_app.b_ai_agent_joined = false;
            memset(g_app.agent_id, 0, AGENT_ID_LEN);
        }
    } else {
        // Network error or timeout: clear state to allow retry
        printf("HTTP request failed: %s\n", esp_err_to_name(err));
        printf("⚠ Clearing state to allow restart\n");
        g_app.b_ai_agent_joined = false;
        memset(g_app.agent_id, 0, AGENT_ID_LEN);
    }

    esp_http_client_cleanup(client);
}

/* Remove old functions that are no longer needed */
void ai_agent_generate(void)
{
    // No longer needed - removed
    printf("ai_agent_generate is deprecated\n");
}

void ai_agent_ping(void)
{
    // No longer needed - removed
    printf("ai_agent_ping is deprecated\n");
}
