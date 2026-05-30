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
    
    if (!p) {
        strcat(newsOut, "Sem dados disponíveis no momento. ");
        return;
    }
    
    p = strstr(p, "<item>");
    
    while (p && count < 6) { 
        char *titleStart = strstr(p, "<title>");
        if (!titleStart) break;
        
        char *nextItem = strstr(p + 6, "<item>");
        if (nextItem && titleStart > nextItem) {
            p = nextItem;
            continue;
        }
        
        titleStart += 7; 
        char *titleEnd = strstr(titleStart, "</title>");
        if (!titleEnd) break;
        
        size_t len = titleEnd - titleStart;
        if (len > 0 && len < 250) {
            char tempTitle[250];
            strncpy(tempTitle, titleStart, len);
            tempTitle[len] = '\0';
            
            char *cdata = strstr(tempTitle, "<![CDATA[");
            if (cdata) {
                char *cdataEnd = strstr(cdata, "]]>");
                if (cdataEnd) {
                    *cdataEnd = '\0';
                    if (strlen(cdata + 9) > 3) {
                        strcat(newsOut, cdata + 9);
                        strcat(newsOut, " +++ ");
                        count++;
                    }
                }
            } else {
                if (strlen(tempTitle) > 3) {
                    strcat(newsOut, tempTitle);
                    strcat(newsOut, " +++ ");
                    count++;
                }
            }
        }
        p = strstr(titleEnd, "<item>"); 
    }
    
    if (count == 0) {
        strcat(newsOut, "Buscando novas manchetes... Aguarde o próximo ciclo. ");
    }
}

void GetData(WeatherData *w, char *news) {
    CURL *curl = curl_easy_init();
    if (!curl) return;

    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    int current_hour = timeinfo->tm_hour;

    static int sourceIndex = 0;

    MemoryString chunk = { malloc(1), 0 };
    const char *weather_url = "https://api.open-meteo.com/v1/forecast?latitude=-19.92&longitude=-43.94&current_weather=true&hourly=temperature_2m,weathercode&daily=sunrise,sunset&timezone=auto";
    
    curl_easy_setopt(curl, CURLOPT_URL, weather_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Arch Linux; Linux x86_64)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (curl_easy_perform(curl) == CURLE_OK) {
        char *cw = strstr(chunk.payload, "\"current_weather\"");
        if (cw) {
            char *temp_ptr = strstr(cw, "\"temperature\":");
            if (temp_ptr) w->temp = strtof(temp_ptr + 14, NULL);
        }

        char *h_ptr = strstr(chunk.payload, "\"temperature_2m\":[");
        if (h_ptr) {
            char *p = strchr(h_ptr, '[');
            for (int i = 0; i < current_hour; i++) {
                if (p) { p++; strtof(p, &p); p = strchr(p, ','); }
            }
            for (int i = 0; i < 3; i++) {
                if (p) {
                    p++;
                    w->forecast_temp[i] = strtof(p, &p);
                    p = strchr(p, ',');
                } else {
                    w->forecast_temp[i] = w->temp; 
                }
            }
        }

        char *sr = strstr(chunk.payload, "\"sunrise\":[\"");
        if (sr) { strncpy(w->sunrise, sr + 23, 5); w->sunrise[5] = 0; }
        char *ss = strstr(chunk.payload, "\"sunset\":[\"");
        if (ss) { strncpy(w->sunset, ss + 22, 5); w->sunset[5] = 0; }
    }

    free(chunk.payload);
    chunk.payload = malloc(1);
    chunk.size = 0;

    const char *news_url = "";
    const char *sourceName = "";

    if (sourceIndex == 0) { 
    } else if (sourceIndex == 1) {
        news_url = "https://feeds.bbci.co.uk/portuguese/rss.xml";
        sourceName = "BBC BRASIL";
    } else {
        news_url = "https://www.metropoles.com/feed";
        sourceName = "METRÓPOLES";
    }

    curl_easy_setopt(curl, CURLOPT_URL, news_url);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); 
    
    if (curl_easy_perform(curl) == CURLE_OK) {
        ParseNewsRSS(chunk.payload, news, sourceName);
    } else {
        sprintf(news, " +++ ERRO AO CONECTAR NAS NOTÍCIAS DA %s +++ ", sourceName);
    }

    sourceIndex = (sourceIndex + 1) % 3;

    curl_easy_cleanup(curl);
    free(chunk.payload);
}

int main(void) {
    setlocale(LC_ALL, "pt_BR.UTF-8");
    InitWindow(1050, 1680, "RayDash");
    SetTargetFPS(60);

    WeatherData w = {0};
    char newsTxt[4096] = {0}, uptimeTxt[32] = {0}; 
    float scrollX = 1050;
    int updateCounter = 0;

    GetData(&w, newsTxt);

    while (!WindowShouldClose()) {
        updateCounter++;
        if (updateCounter >= 1800) { GetData(&w, newsTxt); updateCounter = 0; }
        
        GetUptime(uptimeTxt);
        int ram = GetRAMLinux();

        scrollX -= 2.0f; 
        if (scrollX < -MeasureText(newsTxt, 35)) scrollX = 1050;

        BeginDrawing();
            ClearBackground(BLACK);

            time_t n = time(NULL); struct tm *t = localtime(&n);
            char timeStr[10], dateStr[64];
            strftime(timeStr, 10, "%H:%M", t);
            strftime(dateStr, 64, "%A, %d de %B", t);

            // Cabeçalho
            DrawText(timeStr, 80, 150, 260, RAYWHITE);
            DrawText(TextFormat("%02d", t->tm_sec), 830, 310, 70, SKYBLUE);
            DrawText(dateStr, 85, 430, 35, GRAY);

            // Clima e LUA
            DrawRectangle(80, 550, 890, 1, DARKGRAY);
            DrawText(TextFormat("%.1f°C", w.temp), 80, 620, 140, RAYWHITE);
            DrawText(GetMoonPhase(), 600, 640, 28, SKYBLUE);
            DrawText(TextFormat("SOL: %s / %s", w.sunrise, w.sunset), 600, 685, 22, GRAY);

            // Previsão
            for(int i=0; i<3; i++) {
                int x = 80 + (i * 310);
                DrawRectangleLines(x, 820, 280, 180, (Color){40, 40, 40, 255});
                DrawText(TextFormat("+%dh", i+1), x + 20, 840, 20, GRAY);
                DrawText(TextFormat("%.1f°", w.forecast_temp[i]), x + 20, 875, 50, RAYWHITE);
            }

            // Sistema
            DrawText("RECURSOS", 80, 1450, 20, GRAY);
            DrawRectangle(80, 1480, 890, 12, (Color){30, 30, 30, 255});
            DrawRectangle(80, 1480, (890 * ram / 100), 12, ram > 90 ? RED : SKYBLUE);
            DrawText(uptimeTxt, 750, 1505, 22, GRAY);
            DrawText(TextFormat("RAM: %d%%", ram), 80, 1505, 22, RAYWHITE);

            // Letreiro
            DrawRectangle(0, 1610, 1050, 70, (Color){10, 10, 10, 255});
            DrawText(newsTxt, (int)scrollX, 1632, 32, RAYWHITE);

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
