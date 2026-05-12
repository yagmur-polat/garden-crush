#include <raylib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>   // sprintf için
#include <sys/stat.h> // Add for stat()
#include <stdarg.h>
#include <ctype.h> // For isprint

// --- Oyun tahtası ve şekerler ile ilgili sabitler ---
#define BOARD_WIDTH 10      // Tahta genişliği (hücre sayısı)
#define BOARD_HEIGHT 14     // Tahta yüksekliği (hücre sayısı)
#define CELL_SIZE 64        // Her bir hücrenin piksel boyutu
#define NUM_CANDY_TYPES 5   // Şeker türü sayısı
#define SPECIAL_BUCKET 5    // Özel "buket" şekerinin tipi

Font gameFont; // Oyun için özel font

// Oyun durumlarını belirten enum
typedef enum { MENU, LEVEL_SELECT, GAME, LEVEL_COMPLETE, GAME_OVER, SCORES_SCREEN } GameState;

// --- Seviye sistemi ile ilgili sabitler ve yapı ---
#define LEVEL_FOLDER   "levels/"   // Seviye dosyalarının bulunduğu klasör
#define MAX_LEVELS     100         // Maksimum seviye sayısı

// Seviye bilgilerini tutan yapı
typedef struct {
    int targetScore;   // Hedef skor
    int maxMoves;      // Maksimum hamle sayısı
    int levelNumber;   // Seviye numarası
    int timeLimit;     // Süreli seviye için süre (saniye)
} LevelInfo;

// --- Oyuncu verisi ile ilgili sabitler ve yapı ---
#define PLAYERDATA_FILE "playerdata.txt"
#define PLAYER_NAME_MAXLEN 32

typedef struct {
    char name[PLAYER_NAME_MAXLEN]; // Oyuncu adı
    int highScore;                 // En yüksek skor
} PlayerData;

PlayerData playerData = { "", 0 }; // Global oyuncu verisi

// --- Tüm oyuncu skorları için dosya ve yapı ---
#define PLAYERSCORES_FILE "playerscores.txt"
#define MAX_SCORES 100

typedef struct {
    char name[PLAYER_NAME_MAXLEN]; // Oyuncu adı
    int score;                     // Skor
} ScoreEntry;

ScoreEntry scoreEntries[MAX_SCORES]; // Skor geçmişi dizisi
int scoreEntryCount = 0;             // Kayıtlı skor sayısı

// --- Oyuncu verisini dosyadan yükler ---
void LoadPlayerData() {
    FILE* f = fopen(PLAYERDATA_FILE, "r"); // "r" -> okuma modu
    if (f) {
        fgets(playerData.name, PLAYER_NAME_MAXLEN, f);
        // Satır sonu karakterini sil
        size_t len = strlen(playerData.name);
        if (len > 0 && playerData.name[len - 1] == '\n') playerData.name[len - 1] = 0;
        fscanf_s(f, "%d ", &playerData.highScore);
        fclose(f);
    }
    else {
        playerData.name[0] = 0;
        playerData.highScore = 0;
    }
}

// --- Oyuncu verisini dosyaya kaydeder ---
void SavePlayerData() {
    FILE* f = fopen(PLAYERDATA_FILE, "w"); // "w" -> yazma modu
    if (f) {
        fprintf(f, "%s\n%d ", playerData.name, playerData.highScore);
        fclose(f);
    }
}

// --- Tüm skorları dosyadan yükler ---
void LoadAllScores() {
    scoreEntryCount = 0;
    FILE* f = fopen(PLAYERSCORES_FILE, "r"); // "r" -> okuma modu: dosyayı sadece okumak için açar, yazmaya izin vermez
    if (f) {
        while (fgets(scoreEntries[scoreEntryCount].name, PLAYER_NAME_MAXLEN, f)) {
            // Satır sonu karakterini sil
            size_t len = strlen(scoreEntries[scoreEntryCount].name);
            if (len > 0 && scoreEntries[scoreEntryCount].name[len - 1] == '\n')
                scoreEntries[scoreEntryCount].name[len - 1] = 0;
            if (fscanf_s(f, "%d\n", &scoreEntries[scoreEntryCount].score) == 1) {
                scoreEntryCount++;
                if (scoreEntryCount >= MAX_SCORES) break;
            }
        }
        fclose(f);
    }
}

// --- Yeni bir skoru dosyaya ekler ---
void SaveNewScore(const char* name, int score) {
    // Dosyayı ekleme modunda aç ("a" -> append mode: dosya yoksa oluşturur, varsa sonuna veri ekler)
    FILE* f = fopen(PLAYERSCORES_FILE, "a");
    if (f) {
        fprintf(f, "%s\n%d\n", name, score);
        fclose(f);
    }
    else {
        // Hata mesajı ekle: dosya açılamadıysa bildir
        printf("HATA: %s dosyasına skor yazılamadı!\n", PLAYERSCORES_FILE);
    }
}

// --- Oyuncu adı girişi için durumlar ---
bool nameInputActive = false;                  // Ad girişi aktif mi?
char nameInputBuffer[PLAYER_NAME_MAXLEN] = ""; // Geçici ad girişi
int nameInputLen = 0;                          // Girilen karakter sayısı

// --- Seviye ile ilgili global değişkenler ---
LevelInfo currentLevelInfo;      // Şu anki seviye bilgisi
int       currentLevelNumber = 1;// Başlangıç seviyesi
int       levelCount = 0;        // Toplam seviye sayısı

// --- Süreli seviye için global değişkenler ---
float levelTimer = 0.0f;         // Kalan süre
bool timerActive = false;        // Sayaç aktif mi?

// --- Ses ayarları ---
bool soundEnabled = true;        // Efekt sesi açık mı?
bool musicEnabled = true;        // Müzik açık mı?
extern float masterVolume = 1.0f;// Ana ses seviyesi

// --- Şeker (candy) yapısı, sarmaşık (ivy) özelliği dahil ---
typedef struct {
    int type;           // Şeker türü
    bool isMatched;     // Eşleşti mi?
    float yOffset;      // Animasyon için dikey kayma
    float opacity;      // Görünürlük (animasyon için)
    bool isSpecial;     // Özel mi? (buket)
    bool hasIvy;        // Sarmaşık var mı?
} Candy;

// --- Ekran durumları için enum ---
typedef enum {
    MAIN_MENU,
    GAME_PLAY,
    SETTINGS,
    PAUSE_MENU
} GameScreen;

GameScreen currentScreen = MAIN_MENU; // Şu anki ekran
Rectangle soundButtonRect = { 750, 10, 40, 40 }; // Ses butonu alanı
Rectangle musicButtonRect = { 700, 10, 40, 40 }; // Müzik butonu alanı
Rectangle returnMenu