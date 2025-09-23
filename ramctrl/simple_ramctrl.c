#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int main(int argc, char *argv[]) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        printf("Commands: health, add-node, status\n");
        return 1;
    }

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        if (strcmp(argv[1], "health") == 0) {
            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8008/api/v1/cluster/health");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
            
            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                printf("Health Status: %s\n", chunk.memory);
            }
        } else if (strcmp(argv[1], "add-node") == 0) {
            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8008/api/v1/cluster/add-node");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"node_id\": 2, \"hostname\": \"127.0.0.1\", \"address\": \"127.0.0.1\", \"port\": 5433}");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_slist_append(NULL, "Content-Type: application/json"));
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
            
            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                printf("Add Node Result: %s\n", chunk.memory);
            }
        } else {
            printf("Unknown command: %s\n", argv[1]);
        }
        
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    free(chunk.memory);
    return 0;
}
