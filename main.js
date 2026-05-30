#include <raylib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <locale.h>
#include <math.h>

typedef struct { char *payload; size_t size; } MemoryString;
typedef struct { 
    float temp; 
    float forecast_temp[3];
    int forecast_code[3];
    char sunrise[10], sunset[10]; 
} WeatherData;

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryString *mem = (MemoryString *)userp;
    char *ptr = realloc(mem->payload, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->payload = ptr;
    memcpy(&(mem->payload[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->payload[mem->size] = 0;
    return realsize;
}

void GetUptime(char *buffer) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp) {
        float seconds;
        if (fscanf(fp, "%f", &seconds) > 0) {
            int h = (int)seconds / 3600;
            int m = ((int)seconds % 3600) / 60;
            sprintf(buffer, "UPTIME: %dh %dm", h, m);
        }
        fclose(fp);
    }
}

int GetRAMLinux() {
    FILE *fp = fopen("/proc/meminfo", "r");
    long total = 0, freeM = 0, buff = 0, cach = 0;
    char line[256];
    while (fp && fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &total));
        if (sscanf(line, "MemFree: %ld kB", &freeM));
        if (sscanf(line, "Buffers: %ld kB", &buff));
        if (sscanf(line, "Cached: %ld kB", &cach));
    }
    if (fp) fclose(fp);
    return (total > 0) ? (int)(100 - (100 * (freeM + buff + cach) / total)) : 0;
}

const char* GetMoonPhase() {
    time_t now = time(NULL);
    double diff = difftime(now, 1704945600); 
    double cycle = 2551443; 
    double phase = fmod(diff, cycle) / cycle;
    if (phase < 0.06 || phase > 0.94) return "LUA NOVA";
    if (phase < 0.19) return "LUA CRESCENTE";
    if (phase < 0.31) return "QUARTO CRESCENTE";
    if (phase < 0.44) return "GIBOSA CRESCENTE";
    if (phase < 0.56) return "LUA CHEIA";
    if (phase < 0.69) return "GIBOSA MINGUANTE";
    if (phase < 0.81) return "QUARTO MINGUANTE";
    return "LUA MINGUANTE";
}

void ParseNewsRSS(const char *xml, char *newsOut, const char *sourceName) {
    sprintf(newsOut, " +++ NOTÍCIAS AO VIVO [%s]: ", sourceName);
    char *p = (char *)xml;
    int count = 0;
    
    if (!p) return;
    p = strstr(p, "<item>"); // Pula cabeçalhos gerais do feed
    
    while (p && count < 6) { 
        char *titleStart = strstr(p, "<title>");
        if (!titleStart) break;
        titleStart += 7; 
        
        char *titleEnd = strstr(titleStart, "</title>");
        if (!titleEnd) break;
        
        size_t len = titleEnd - titleStart;
        if (len > 0 && len < 200) {
            char tempTitle[200];
            strncpy(tempTitle, titleStart, len);
            tempTitle[len] = '\0';
            
            // Remove marcações CDATA que aparecem muito no G1 e Metrópoles
            char *cdata = strstr(tempTitle, "<![CDATA[");
            if (cdata) {
                char *cdataEnd = strstr(cdata, "]]>");
                if (cdataEnd) {
                    *cdataEnd = '\0';
                    // Garante que não jogue lixo vazio se o CDATA bugar
                    if (strlen(cdata + 9) > 3) strcat(newsOut, cdata + 9);
                }
            } else {
                strcat(newsOut, tempTitle);
            }
            strcat(newsOut, " +++ ");
            count++;
        }
        p = strstr(titleEnd, "<item>"); 
    }
}

void GetData(WeatherData *w, char *news) {
    CURL *curl = curl_easy_init();
    if (!curl) return;

    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    int current_hour = timeinfo->tm_hour;

    // Controla qual portal será chamado a cada ciclo de atualização (0=G1, 1=BBC, 2=Metrópoles)
    static int sourceIndex = 0;

    MemoryString chunk = { malloc(1), 0 };
    const char *weather_url = "https://api.open-meteo.com/v1/forecast?latitude=-19.92&longitude=-43.94&current_weather=true&hourly=temperature_2m,weathercode&daily=sunrise,sunset&timezone=auto";
    
    curl_easy_setopt(curl, CURLOPT_URL, weather_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // 1. Coleta de Clima
    if (curl_easy_perform(curl) == CURLE_OK) {
        char *cw = strstr(chunk.payload, "\"current_weather
