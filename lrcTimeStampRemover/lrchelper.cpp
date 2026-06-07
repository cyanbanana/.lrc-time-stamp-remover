#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Round_Button.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Progress.H>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
#include <regex>
#include <string>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>   
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

namespace fs = std::filesystem;

enum Lang { LANG_ZH = 0, LANG_EN = 1, LANG_JP = 2 };
Lang currentLang = LANG_ZH;

const char* uiTexts[3][17] = {
    {
        "LRC 时间戳移除工具",
        "LRC 文件夹",
        "浏览",
        "输出文件夹",
        "留空则输出在输入文件夹",
        "转换",
        "选择 LRC 文件夹",
        "输入文件夹路径不能为空",
        "输入文件夹不存在",
        "输入路径不是有效目录",
        "输出文件夹不存在",
        "输入文件夹内没找到 .lrc 文件",
        "转换完成！",
        "语言",
        "中文",
        "English",
        "日本語",
    },
    {
        "LRC Time Stamp Remover",
        "LRC Folder",
        "Browse",
        "Output Folder",
        "Leave blank to output in input folder",
        "Convert",
        "Select LRC folder",
        "Input folder path cannot be empty.",
        "Input folder does not exist.",
        "Input path is not a valid directory.",
        "Output folder does not exist.",
        "No .lrc files found in the input folder.",
        "Conversion completed!",
        "Language",
        "Chinese",
        "English",
        "Japanese",
    },
    {
        "LRC タイムスタンプ除去ツール",
        "LRC フォルダ",
        "参照",
        "出力フォルダ",
        "空白の場合は入力フォルダに出力",
        "変換",
        "LRC フォルダを選択",
        "入力フォルダのパスが空です",
        "入力フォルダが存在しません",
        "入力パスが有効なディレクトリではありません",
        "出力フォルダが存在しません",
        "入力フォルダに .lrc ファイルが見つかりません",
        "変換完了！",
        "言語",
        "中国語",
        "英語",
        "日本語",
    }
};

struct Lrcinfo {
    std::string title;
    std::string artist;
    std::string album;
    std::vector<std::string> lyrics;
};

struct PathPair {
    Fl_Input* input;
    Fl_Input* output;
    Fl_Progress* progress;
    Fl_Box* titleBox;
    Fl_Box* inpLabel;
    Fl_Box* outLabel;
    Fl_Button* browseInBtn;
    Fl_Button* browseOutBtn;
    Fl_Button* convertBtn;
    Fl_Box* langLabel;
    Fl_Round_Button* radio_zh;
    Fl_Round_Button* radio_en;
    Fl_Round_Button* radio_jp;
};

void updateUILanguage(PathPair* pp, Lang lang) {
    const char* oldPlaceholder = uiTexts[currentLang][4];
    const char* newPlaceholder = uiTexts[lang][4];

    pp->titleBox->label(uiTexts[lang][0]);
    pp->inpLabel->label(uiTexts[lang][1]);
    pp->browseInBtn->label(uiTexts[lang][2]);
    pp->outLabel->label(uiTexts[lang][3]);
    if (strlen(pp->output->value()) == 0 || strcmp(pp->output->value(), oldPlaceholder) == 0) {
        pp->output->value(newPlaceholder);
    }
    pp->browseOutBtn->label(uiTexts[lang][2]);
    pp->convertBtn->label(uiTexts[lang][5]);
    pp->langLabel->label(uiTexts[lang][13]);
    pp->radio_zh->label(uiTexts[lang][14]);
    pp->radio_en->label(uiTexts[lang][15]);
    pp->radio_jp->label(uiTexts[lang][16]);

    Fl_Window* win = (Fl_Window*)pp->titleBox->parent();
    win->label(uiTexts[lang][0]);

    currentLang = lang;
    Fl::redraw();
}

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = s.end();
    while (end > start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(start, end);
}

std::vector<std::string> getAllLrcFiles(const std::string& directory) {
    std::vector<std::string> lrcfiles;
    fs::path dir(directory);
    for (const auto& entry : fs::directory_iterator(dir)){
        if (entry.is_regular_file() && entry.path().extension() == ".lrc") {
            lrcfiles.push_back(entry.path().string());
        }
    }
    return lrcfiles;
}

bool parselrc(const std::string& filepath, Lrcinfo& info) {
    std::ifstream file{fs::path(filepath)};
    if (!file.is_open()) return false;

    std::string line;
    std::regex meta(R"(\{.*\})");
    std::regex timeregex(R"(\[.*\])");
    
    while (std::getline(file, line)) {
        std::string cleanline = std::regex_replace(line, meta, "");
        cleanline = std::regex_replace(cleanline, timeregex, "");
        cleanline = trim(cleanline);
        if (!cleanline.empty()) info.lyrics.push_back(cleanline);
    }
    return true;
}

void generateTxt(const Lrcinfo& info, const std::string& outputPath) {
    std::ofstream out{fs::path(outputPath)};
    if (!out.is_open()) {
        std::cerr << "Failed to write file: " << outputPath << std::endl;
        return;
    }
    if (!info.title.empty()) {
        out << "[" << info.title << "]" << std::endl;
        out << std::string(50, '=') << std::endl;
    }
    for (const auto& line : info.lyrics) {
        out << line << std::endl;
    }
}

void convertFile(const std::string& ipt, const std::string& opt) {
    Lrcinfo info;
    if (!parselrc(ipt, info)) {
        std::cerr << "Failed to parse: " << ipt << std::endl;
        return;
    }
    info.title = fs::path(ipt).stem().string();
    fs::path inPath(ipt);
    fs::path outPath = fs::path(opt) / (inPath.stem().string() + ".txt");
    generateTxt(info, outPath.string());
}

void whenbuttonclicked(Fl_Widget* widget, void* data) {
    PathPair* pp = static_cast<PathPair*>(data);
    std::string ipt = pp->input->value();
    std::string opt = pp->output->value();

    if (ipt.empty()) {
        fl_message(uiTexts[currentLang][7]);
        return;
    }
    if (!fs::exists(ipt)) {
        fl_message(uiTexts[currentLang][8]);
        return;
    }
    if (!fs::is_directory(ipt)) {
        fl_message(uiTexts[currentLang][9]);
        return;
    }

    const char* placeholder = uiTexts[currentLang][4];
    if (opt.empty() || opt == placeholder) {
        opt = ipt;
    } else if (!fs::exists(opt) || !fs::is_directory(opt)) {
        fl_message(uiTexts[currentLang][10]);
        return;
    }

    std::vector<std::string> lrcfiles = getAllLrcFiles(ipt);
    int total = lrcfiles.size();
    if (total == 0) {
        fl_message(uiTexts[currentLang][11]);
        return;
    }

    pp->progress->maximum(total);
    for (int i = 0; i < total; ++i) {
        convertFile(lrcfiles[i], opt);
        pp->progress->value(i + 1);
        Fl::check();
    }
    fl_message(uiTexts[currentLang][12]);
    pp->progress->value(0);
}

void browse_callback(Fl_Widget* w, void* data) {
    Fl_Native_File_Chooser chooser1;
    chooser1.title(uiTexts[currentLang][6]);
    chooser1.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
    chooser1.filter("All files\t*.*");
    if (chooser1.show() == 0) {
        const char* filepath = chooser1.filename();
        Fl_Input* input = static_cast<Fl_Input*>(data);
        input->value(filepath);
    }
}

void lang_cb(Fl_Widget* w, void* data) {
    Lang lang = static_cast<Lang>((intptr_t)data);
    Fl_Window* win = (Fl_Window*)w->parent();          // 直接获取窗口
    PathPair* pp = static_cast<PathPair*>(win->user_data());
    updateUILanguage(pp, lang);
}

int main() {
    Fl_Window win(800, 650, uiTexts[currentLang][0]);

    Fl_Box* titleBox = new Fl_Box(70, 10, 660, 50, uiTexts[currentLang][0]);
    titleBox->labelsize(32);

    Fl_Box* inpLabel = new Fl_Box(80, 170, 150, 37, uiTexts[currentLang][1]);
    inpLabel->labelsize(25);
    Fl_Input* input = new Fl_Input(240, 170, 370, 37);

    Fl_Button* browseInBtn = new Fl_Button(620, 170, 80, 37, uiTexts[currentLang][2]);
    browseInBtn->callback(browse_callback, input);

    Fl_Box* outLabel = new Fl_Box(80, 224, 150, 37, uiTexts[currentLang][3]);
    outLabel->labelsize(25);
    Fl_Input* output = new Fl_Input(240, 224, 370, 37);
    output->value(uiTexts[currentLang][4]);

    Fl_Button* browseOutBtn = new Fl_Button(620, 224, 80, 37, uiTexts[currentLang][2]);
    browseOutBtn->callback(browse_callback, output);

    Fl_Box* langLabel = new Fl_Box(80, 278, 150, 37, uiTexts[currentLang][13]);
    langLabel->labelsize(25);

    Fl_Round_Button* radio_zh = new Fl_Round_Button(240, 278, 110, 37, uiTexts[currentLang][14]);
    Fl_Round_Button* radio_en = new Fl_Round_Button(360, 278, 110, 37, uiTexts[currentLang][15]);
    Fl_Round_Button* radio_jp = new Fl_Round_Button(480, 278, 110, 37, uiTexts[currentLang][16]);
    radio_zh->type(FL_RADIO_BUTTON);
    radio_en->type(FL_RADIO_BUTTON);
    radio_jp->type(FL_RADIO_BUTTON);
    radio_zh->setonly();

    Fl_Progress* progress = new Fl_Progress(70, 430, 660, 30);
    progress->minimum(0);
    progress->maximum(100);
    progress->value(0);
    progress->selection_color(FL_BLUE);
    progress->color(FL_YELLOW);

    Fl_Button* convertBtn = new Fl_Button(70, 370, 660, 40, uiTexts[currentLang][5]);
    convertBtn->labelsize(20);

    PathPair pp;
    pp.input = input;
    pp.output = output;
    pp.progress = progress;
    pp.titleBox = titleBox;
    pp.inpLabel = inpLabel;
    pp.outLabel = outLabel;
    pp.browseInBtn = browseInBtn;
    pp.browseOutBtn = browseOutBtn;
    pp.convertBtn = convertBtn;
    pp.langLabel = langLabel;
    pp.radio_zh = radio_zh;
    pp.radio_en = radio_en;
    pp.radio_jp = radio_jp;

    win.user_data(&pp);

    radio_zh->callback(lang_cb, (void*)LANG_ZH);
    radio_en->callback(lang_cb, (void*)LANG_EN);
    radio_jp->callback(lang_cb, (void*)LANG_JP);

    convertBtn->callback(whenbuttonclicked, &pp);

    win.end();
    win.show();
    return Fl::run();
}