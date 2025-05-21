#include "crow_all.h"
#include <fstream>
#include <sstream>
#include <codecvt>
#include <regex>
#include <gxsdldr.cpp>
#include <cmanpr.h>
#include <string>
#include <vector>

// Funzione base64 → std::vector<unsigned char>
static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(const std::string& encoded_string) {
    static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

    int in_len = encoded_string.size();
    int i = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j <4; j++)
            char_array_4[j] = 0;

        for (int j = 0; j <4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int j = 0; j < i - 1; j++)
            ret += char_array_3[j];
    }

    return ret;
}

// Funzione per salvare buffer in un file temporaneo
std::string save_temp_image(const std::vector<unsigned char>& buffer) {
    std::string filename = "/tmp/uploaded_image.jpg";
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return filename;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/analyze").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        try {
            auto body = crow::json::load(req.body);
            if (!body || !body.has("image")) {
                return crow::response(400, "Missing 'image' field");
            }

            std::string base64_image = body["image"].s();

            // Rimuove intestazione data URI, se presente
            std::regex header_regex("^data:image/[^;]+;base64,");
            base64_image = std::regex_replace(base64_image, header_regex, "");

            std::string decoded = base64_decode(base64_image);

            std::string tmp_file = save_temp_image(std::vector<unsigned char>(decoded.begin(), decoded.end()));

            // Usa la libreria ANPR
            cmAnpr anpr("eur");
            gxImage image;
            image.Load(tmp_file.c_str());

            if (!anpr.FindFirst(image)) {
                return crow::response(200, "No plate found");
            }

            std::ostringstream result;
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

            do {
                auto coord = anpr.GetResult()->frame;
                auto plate = converter.to_bytes(anpr.GetText());
                auto country = converter.to_bytes(anpr.GetCountryCode(anpr.GetType(), (int) CCT_COUNTRY_SHORT));
                result << plate << " " << country << " " << anpr.GetConfidence();
                result << " (" << coord.x1 << "," << coord.y1 << ")";
                result << " (" << coord.x2 << "," << coord.y2 << ")";
                result << " (" << coord.x3 << "," << coord.y3 << ")";
                result << " (" << coord.x4 << "," << coord.y4 << ")";
                result << " " << image.xsize() << " " << image.ysize() << "\n";
            } while (anpr.FindNext());

            return crow::response(200, result.str());

        } catch (const std::exception& e) {
            return crow::response(500, std::string("Internal error: ") + e.what());
        }
    });

    CROW_ROUTE(app, "/docs")([] {
        return "<html><body><h1>HunghereseWeb API</h1><p>POST /analyze con JSON: {\"image\": \"base64_dell'immagine\"}</p></body></html>";
    });

    app.port(18080).multithreaded().run();
}
