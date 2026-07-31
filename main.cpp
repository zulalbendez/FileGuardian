#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include "sqlite3.h"
//c++17= kuruluma gerek yok ; kütüphaneleri bütün işletim sistemlerinde aynı şekilde  çalışıyor 
namespace fs = std::filesystem;
//SQLite projeye dahil edilen dosyalarla basitce veri saklayan veritabanı , birden fazla etiketleme sunar.
using namespace std;

// TETIKLEYICI KELIME LISTESI (risk analizi)
static const vector< pair<string, string>> triggerWords = {
    {"sifre", "DUSUK"},
    {"parola", "DUSUK"},
    {"password", "DUSUK"},
    {"para", "DUSUK"},
    {"tc kimlik", "YUKSEK"},
    {"kimlik no", "YUKSEK"},
    {"kredi karti", "YUKSEK"},
    {"kart no ", " ORTA"},
    {"iban", "ORTA"},
    {"gizli", "ORTA"},
    {"confidential", "ORTA"}
};



// Kucuk harfe cevirme (karsilastirma icin)
string toLower(const string& s) {
    string result = s;
    for (auto& c : result) c = tolower(static_cast<unsigned char>(c));
    return result;
}

// Risk kelimesini sayiya cevirir, karsilastirma yapabilmek icin
// (YUKSEK > ORTA > DUSUK)
int riskSeviyesi(const string& risk) {
    if (risk == "YUKSEK") return 3;
    if (risk == "ORTA") return 2;
    if (risk == "DUSUK") return 1;
    return 0;
}

// Uzantiya gore genel etiket dondurur
string getExtensionTag(const string& extension) {
    if (extension == ".csv" || extension == ".xlsx" || extension == ".json") return "veri";
    if (extension == ".txt") return "metin";
    if (extension == ".docx" || extension == ".doc" || extension == ".pdf") return "dokuman";
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") return "gorsel";
    if (extension == ".cpp" || extension == ".h" || extension == ".py") return "kod";
    return "diger";
}

// Bu uzantidaki dosyalarin icerigi metin olarak okunabilir mi?
bool isTextReadable(const string& extension) {
    return extension == ".txt" || extension == ".csv" || extension == ".json"
        || extension == ".xml" || extension == ".log" || extension == ".md";
}


// ANA MANTIK: Bir dosya icin TUM etiketleri hesaplayip
// tek bir string olarak dondurur. Ornek sonuc: "dokuman, ozel_isim"

string hesaplaEtiketler(const string& filename, const string& extension) {
    string etiketler = "";

    // Katman 1: uzanti bazli etiket
    etiketler += getExtensionTag(extension);

    // Katman 2: dosya adi buyuk harfle basliyor mu?
    if (!filename.empty() && isupper(static_cast<unsigned char>(filename[0]))) {
        etiketler += ", ozel_isim";
    }

    // Katman 2b: dosya adinda "fatura" veya "invoice" geciyor mu?
    string lowerName = toLower(filename);
    if (lowerName.find("fatura") != string::npos || lowerName.find("invoice") != string::npos) {
        etiketler += ", fatura";
    }

    return etiketler;
}

// ICERIK TARAMASI: dosyanin icini tetikleyici kelimelere karsi tarar
// En yuksek risk seviyesini ve bulunan kelimeleri dondurur

struct TaramaSonucu {
    string riskSeviyesiStr = "YOK";
    string bulunanKelimeler = "";
};

TaramaSonucu icerikTara(const string& filepath) {
    TaramaSonucu sonuc;

    ifstream dosya(filepath);
    if (!dosya.is_open()) {
        return sonuc; // dosya acilamadiysa "YOK" olarak dondur
    }

    // Dosyanin tum icerigini tek bir string'e oku
    stringstream buffer;
    buffer << dosya.rdbuf();
    string icerik = toLower(buffer.str());
    dosya.close();

    int enYuksekRisk = 0;

    // Listedeki her kelimeyi tek tek kontrol et
    for (const auto& kelimeRiskCifti : triggerWords) {
        string kelime = kelimeRiskCifti.first;
        string risk = kelimeRiskCifti.second;

        if (icerik.find(toLower(kelime)) != string::npos) {
            // Kelime bulundu, listeye ekle
            if (!sonuc.bulunanKelimeler.empty()) sonuc.bulunanKelimeler += ", ";
            sonuc.bulunanKelimeler += kelime;

            // Bu kelimenin riski, su ana kadarki en yuksek riskten fazla mi?
            if (riskSeviyesi(risk) > enYuksekRisk) {
                enYuksekRisk = riskSeviyesi(risk);
                sonuc.riskSeviyesiStr = risk;
            }
        }
    }

    return sonuc;
}

// Zaman bilgisini okunabilir stringe cevirir
string formatTime(fs::file_time_type ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    string timeStr = ctime(&cftime);
    if (!timeStr.empty() && timeStr.back() == '\n') timeStr.pop_back();
    return timeStr;
}


// VERITABANI: TEK bir tablo olusturuyoruz (files).
// Etiketler ayri tabloda degil, "tags" adli TEK bir metin kolonunda,
// virgulle ayrilmis sekilde tutuluyor. Boylece ekstra iliski
// tablosuna gerek kalmiyor.
bool veritabaniHazirla(sqlite3* db) {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "filename TEXT,"
        "filepath TEXT,"
        "extension TEXT,"
        "size_kb REAL,"
        "modified_date TEXT,"
        "tags TEXT,"              
        "risk_level TEXT,"        
        "found_keywords TEXT"    
        ");";

    char* hataMesaji = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &hataMesaji) != SQLITE_OK) {
        cerr << "Tablo olusturma hatasi: " << hataMesaji << endl;
        sqlite3_free(hataMesaji);
        return false;
    }
    return true;
}

// Tek bir dosyanin tum bilgilerini veritabanina ekler.SQLite kullnama şartı   
// "?" isaretleri, degerlerin guvenli sekilde yerlestirilecegi yerler.
void dosyaEkle(sqlite3* db, const string& filename, const string& filepath,
               const string& extension, double sizeKB, const string& modDate,
               const string& tags, const string& risk, const string& kelimeler) {

    const char* sql =//files tablosuna şu kolonlara şu değerleri ekle
        "INSERT INTO files (filename, filepath, extension, size_kb, modified_date, tags, risk_level, found_keywords) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    // Her "?" isaretine sirayla degerleri yerlestiriyoruz
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, filepath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, extension.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, sizeKB);
    sqlite3_bind_text(stmt, 5, modDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, tags.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, risk.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, kelimeler.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);       // komutu calistir
    sqlite3_finalize(stmt);   // temizle
}


// ANA PROGRAM

int main(int argc, char* argv[]) {

    // 1. Kullanici klasor yolu vermis mi kontrol et
    if (argc < 2) {
        cerr << "Kullanim: " << argv[0] << " <klasor_yolu>" << endl;
        return 1;
    }
    string rootPath = argv[1];

    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        cerr << "Hata: Gecersiz klasor yolu: " << rootPath << endl;
        return 1;
    }

    // 2. Veritabanini ac (yoksa dosyayi otomatik olusturur)
    sqlite3* db;
    if (sqlite3_open("veriler.db", &db) != SQLITE_OK) {
        cerr << "Veritabani acilamadi." << endl;
        return 1;
    }

    // 3. Tabloyu hazirla
    if (!veritabaniHazirla(db)) {
        sqlite3_close(db);
        return 1;
    }
    // Her calistirmada eski kayitlari temizle, boylece ayni klasoru
// tekrar tekrar taramak ayni dosyalari coklamasin
    sqlite3_exec(db, "DELETE FROM files;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "DELETE FROM sqlite_sequence WHERE name='files';", nullptr, nullptr, nullptr);

    int dosyaSayisi = 0;
    int riskliSayisi = 0;

    // 4. Klasordeki (ve alt klasorlerdeki) her dosyayi gez
    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {

            if (!fs::is_regular_file(entry.path())) continue; // sadece dosyalarla ilgileniyoruz

            string filename = entry.path().filename().string();
            string fullPath = entry.path().string();
            string extension = entry.path().extension().string();
            double sizeKB = fs::file_size(entry.path()) / 1024.0;
            string modTime = formatTime(fs::last_write_time(entry.path()));

            // Dosya adi ve uzantiya gore etiketleri hesapla
            string etiketler = hesaplaEtiketler(filename, extension);

            // Icerigi metin olarak okuyabiliyorsak, riskli kelime taramasi yap
            TaramaSonucu tarama;
            if (isTextReadable(extension)) {
                tarama = icerikTara(fullPath);
            }

            // Risk bulunduysa, ekrana anlik uyari bas
            if (tarama.riskSeviyesiStr != "YOK") {
                riskliSayisi++;
                cout << "[UYARI] " << filename << " -> Risk: " << tarama.riskSeviyesiStr
                     << " (Kelime: " << tarama.bulunanKelimeler << ")" << endl;
            }

            // Her seyi veritabanina tek satirda kaydet
            dosyaEkle(db, filename, fullPath, extension, sizeKB, modTime,
                      etiketler, tarama.riskSeviyesiStr, tarama.bulunanKelimeler);

            dosyaSayisi++;
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "Tarama hatasi: " << e.what() << endl;
    }

    sqlite3_close(db);

    // 5. Ozet bilgi yazdir
    cout << "\nTamamlandi! " << dosyaSayisi << " dosya tarandi." << endl;
    cout << riskliSayisi << " dosyada riskli icerik tespit edildi." << endl;
    cout << "Sonuclar 'veriler.db' veritabanina yazildi." << endl;

    return 0;
}
