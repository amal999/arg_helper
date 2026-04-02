/*
  arg_helper - JSON facts 版本：支持中文/英文，本地 facts.json，常用回应，剪贴板
  依赖：nlohmann::json (https://github.com/nlohmann/json) - header-only 或系统包
  编译（示例）：
    使用系统安装的 nlohmann_json（Debian/Ubuntu）:
      g++ -std=c++17 main.cpp -O2 -o arg_helper -I/usr/include
    或者用 CMake (见 CMakeLists.txt)
*/
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

// Utils ---------------------------------------------------------------------
static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a==string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b-a+1);
}

static bool contains_chinese(const string &s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return true;
    }
    return false;
}

static string lower_ascii(const string &s) {
    string t = s;
    for (char &c : t) if ((unsigned char)c < 128) c = tolower(c);
    return t;
}

// Files for facts and canned responses -------------------------------------
const string FACTS_FILE = "facts.json";
const string CANNED_FILE = "canned_responses.txt";

// facts stored as map<topic, vector<fact_lines>>
unordered_map<string, vector<string>> FACTS;

// canned responses
vector<string> CANNED;

void load_canned(const string &path = CANNED_FILE) {
    CANNED.clear();
    ifstream in(path);
    if (!in) return;
    string line;
    while (getline(in, line)) {
        if (!trim(line).empty()) CANNED.push_back(line);
    }
}
void save_canned(const string &path = CANNED_FILE) {
    ofstream out(path);
    for (auto &s : CANNED) out << s << "\n";
}

// Load facts from JSON. Expected format: array of objects:
// [
//   { "topic":"climate", "lang":"zh", "source":"IPCC, 2021", "text":"事实文本..." },
//   ...
// ]
void load_facts(const string &path = FACTS_FILE) {
    FACTS.clear();
    ifstream in(path);
    if (!in) {
        // no file -> nothing loaded
        return;
    }
    try {
        json j;
        in >> j;
        if (!j.is_array()) return;
        for (auto &entry : j) {
            string topic = entry.value("topic", "general");
            string lang  = entry.value("lang", "auto");
            string source = entry.value("source", "");
            string text = entry.value("text", "");
            string composed = text;
            if (!source.empty()) composed += " (来源: " + source + ")";
            FACTS[topic].push_back(composed);
            // also, if entry contains "aliases" as array, index them too
            if (entry.contains("aliases") && entry["aliases"].is_array()) {
                for (auto &a : entry["aliases"]) {
                    string at = a.get<string>();
                    FACTS[at].push_back(composed);
                }
            }
        }
    } catch (std::exception &e) {
        cerr << "解析 " << path << " 失败: " << e.what() << "\n";
    }
}

// Detection & generation ----------------------------------------------------
static vector<string> EXTREME_EN = {"always","never","everyone","nobody","everybody","impossible","completely","totally","everyone"};
static vector<string> INSULTS_EN = {"stupid","idiot","dumb","trash","loser","crazy"};
// 中文关键词
static vector<string> EXTREME_ZH = {"总是","永远","所有人","绝不","完全","不可能","每个人","没人"};
static vector<string> INSULTS_ZH = {"笨蛋","傻子","蠢","垃圾","白痴","脑残","低能"};

bool is_question(const string &s) {
    string t = trim(s);
    if (t.empty()) return false;
    if (t.back() == '?' || t.back() == '？') return true;
    string tl = lower_ascii(t);
    static vector<string> qwords = {"why","what","how","when","where","who","is","are","do","does","did","would","could","should"};
    for (auto &w : qwords) {
        if (tl.rfind(w + " ", 0) == 0) return true;
    }
    static vector<string> qwords_zh = {"为什么","怎么","如何","吗","是不是","么"};
    for (auto &w : qwords_zh) {
        if (t.find(w) != string::npos) return true;
    }
    return false;
}

bool contains_any_ci(const string &s, const vector<string> &words) {
    string lower_s = lower_ascii(s);
    for (auto &w : words) {
        string lw = lower_ascii(w);
        if (lower_s.find(lw) != string::npos) return true;
    }
    return false;
}

vector<string> detect_issues(const string &s) {
    vector<string> issues;
    bool zh = contains_chinese(s);
    if (zh) {
        for (auto &w : EXTREME_ZH) if (s.find(w) != string::npos) { issues.push_back("包含绝对化/极端化表述（例如：" + w + "）。"); break; }
        for (auto &w : INSULTS_ZH)  if (s.find(w) != string::npos) { issues.push_back("包含侮辱性词汇（例如：" + w + "），可能激化冲突。"); break; }
    } else {
        if (contains_any_ci(s, EXTREME_EN)) issues.push_back("Contains absolute/extreme wording (e.g. always/never).");
        if (contains_any_ci(s, INSULTS_EN))  issues.push_back("Contains insulting language (e.g. stupid/idiot.");
    }
    if (!is_question(s)) {
        bool has_reason = false;
        string lower_s = lower_ascii(s);
        if (lower_s.find("because")!=string::npos || lower_s.find("since")!=string::npos) has_reason = true;
        if (s.find("因为")!=string::npos || s.find("由于")!=string::npos) has_reason = true;
        if (!has_reason) {
            if (zh) issues.push_back("陈述未包含理由或证据，建议要求对方给出支撑。");
            else issues.push_back("Statement lacks evidence/reason; consider asking for supporting data.");
        }
    }
    return issues;
}

// find facts by topic fuzzy match
vector<string> lookup_facts(const string &topic) {
    vector<string> res;
    string t = lower_ascii(topic);
    for (auto &p : FACTS) {
        string key = lower_ascii(p.first);
        if (t.find(key) != string::npos || key.find(t) != string::npos) {
            res.insert(res.end(), p.second.begin(), p.second.end());
        }
    }
    // also try scanning all facts for topic substring (fallback)
    if (res.empty() && !t.empty()) {
        for (auto &p : FACTS) {
            for (auto &f : p.second) {
                string lf = lower_ascii(f);
                if (lf.find(t) != string::npos) res.push_back(f);
            }
        }
    }
    return res;
}

vector<pair<string,string>> generate_responses(const string &topic, const string &input) {
    vector<pair<string,string>> out;
    bool zh = contains_chinese(input) || contains_chinese(topic);

    // 1. 认可 + 澄清
    if (zh) {
        out.emplace_back("认可并澄清", "我理解你的观点：\"" + input + "\"。能否再具体说一下你最关心的点是什么？比如时间、范围或具体例子。);
    } else {
        out.emplace_back("Acknowledge + Clarify", "I hear you: \"" + input + "\". Could you clarify which part you are most concerned about (timeframe, scope, examples)?);
    }

    // 2. 要求证据
    if (zh) {
        out.emplace_back("要求证据", "你提到\"" + input + "\"，能否提供具体来源或数据来支撑？例如研究、统计或者实例。);
    } else {
        out.emplace_back("Ask for evidence", "You mentioned \"" + input + "\"—could you share sources or data supporting that claim (studies, stats, examples)?);
    }

    // 3. 本地事实补充
    auto facts = lookup_facts(topic.empty() ? "general" : topic);
    if (!facts.empty()) {
        string body;
        if (zh) {
            body = "现有资料显示：\n";
            for (auto &f : facts) body += "- " + f + "\n";
            body += "如果你的信息不同，欢迎给出来源，我们可以一起核对。";
            out.emplace_back("事实补充/反驳", body);
        } else {
            body = "Known facts:\n";
            for (auto &f : facts) body += "- " + f + "\n";
            body += "If you have other sources, please share and we can verify together.";
            out.emplace_back("Facts / Counterpoint", body);
        }
    } else {
        if (zh) out.emplace_back("请求来源或协作", "我这边没有关于该话题的本地资料。你可以提供来源，或者我可以帮你整理可查证的信息。);
        else out.emplace_back("Request sources / collaborate", "I don't have local facts for that topic. You can share sources or I can help gather verifiable information.");
    }

    // 4. 指出潜在偏误
    if (zh) out.emplace_back("指出潜在偏误（礼貌）", "你的结论可能存在过度概括或以偏概全的风险。我们可以先约定样本或范围，再讨论具体结论。);
    else out.emplace_back("Point out potential bias (polite)", "Your conclusion might involve overgeneralization. Let's define sample/range before drawing conclusions.");

    // 5. 缓和建议（若情绪化/侮辱）
    bool emo = false;
    for (auto &w : INSULTS_ZH) if (input.find(w)!=string::npos) emo = true;
    for (auto &w : INSULTS_EN) if (contains_any_ci(input, {w})) emo = true;
    if (emo || input.find("!")!=string::npos || input.find("！")!=string::npos) {
        if (zh) out.emplace_back("缓和/重置对话", "现在情绪可能较为激动，建议先冷静一下。要不先把关键问题列出来，再逐条讨论？);
        else out.emplace_back("De-escalate / reset", "The tone seems heated. Maybe take a breath and list key points to discuss one by one?");
    }

    // 6. 反问法（Socratic）
    if (zh) out.emplace_back("反问法（Socratic）", "你会如何证明你的结论？如果对方难以回答，说明结论需要更多证据。);
    else out.emplace_back("Socratic question", "How would you prove your conclusion? If that's hard to answer, you may need more evidence.");

    return out;
}

// Clipboard helpers ---------------------------------------------------------
bool copy_to_clipboard_windows(const string &s) {
#ifdef _WIN32
    if (s.empty()) return false;
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (size + 1) * sizeof(wchar_t));
    if (!hMem) return false;
    wchar_t *wbuf = (wchar_t*)GlobalLock(hMem);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), wbuf, size);
    wbuf[size] = 0;
    GlobalUnlock(hMem);
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
#else
    return false;
#endif
}
bool copy_to_clipboard_unix(const string &s) {
#ifndef _WIN32
    if (s.empty()) return false;
    #ifdef __APPLE__
        FILE* p = popen("pbcopy", "w");
        if (!p) return false;
        fwrite(s.data(), 1, s.size(), p);
        pclose(p);
        return true;
    #else
        if (system(nullptr) == 0) return false;
        FILE* p = popen("xclip -selection clipboard", "w");
        if (p) {
            fwrite(s.data(), 1, s.size(), p);
            pclose(p);
            return true;
        }
        p = popen("xsel --clipboard --input", "w");
        if (p) {
            fwrite(s.data(), 1, s.size(), p);
            pclose(p);
            return true;
        }
        return false;
    #endif
#else
    return false;
#endif
}
bool copy_to_clipboard(const string &s) {
#ifdef _WIN32
    return copy_to_clipboard_windows(s);
#else
    return copy_to_clipboard_unix(s);
#endif
}

// CLI interaction -----------------------------------------------------------
void print_help() {
    cout << "命令帮助：\n";
    cout << "  help          显示帮助\n";
    cout << "  reload facts  重新加载 facts.json\n";
    cout << "  list canned   列出常用回应\n";
    cout << "  save n        将第 n 条回应保存到常用回应\n";
    cout << "  use n         复制第 n 条回应到剪贴板\n";
    cout << "  exit          退出\n";
}
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    load_facts();
    load_canned();

    cout << "arg_helper (JSON facts 版本)\n";
    cout << "输入 'help' 查看命令。输入 'exit' 退出。\n\n";

    while (true) {
        cout << "话题(topic，例如 climate / vaccines / 工作) > ";
        string topic;
        if (!getline(cin, topic)) break;
        topic = trim(topic);
        if (topic == "exit") break;
        if (topic == "help") { print_help(); continue; }
        if (topic == "reload facts") { load_facts(); cout << "已重新加载 " << FACTS_FILE << "\n"; continue; }
        if (topic == "list canned") {
            cout << "常用回应列表：\n";
            for (size_t i=0;i<CANNED.size();++i) cout << i+1 << ". " << CANNED[i] << "\n";
            continue;
        }
        if (topic.rfind("use ",0)==0) {
            string num = trim(topic.substr(4));
            int idx = stoi(num);
            if (idx>=1 && idx <= (int)CANNED.size()) {
                if (copy_to_clipboard(CANNED[idx-1])) cout << "已复制第 " << idx << " 条到剪贴板。\n";
                else cout << "复制到剪贴板失败，请手动复制：\n" << CANNED[idx-1] << "\n";
            } else cout << "索引无效。\n";
            continue;
        }
        if (topic.rfind("save ",0)==0) {
            cout << "请先在一次交互中生成回应，然后使用 save n 保存第 n 条回应。\n";
            continue;
        }

        cout << "对方的陈述或问题 > ";
        string input;
        if (!getline(cin, input)) break;
        input = trim(input);
        if (input == "exit") break;
        if (input == "help") { print_help(); continue; }

        if (input == "reload facts") { load_facts(); cout << "已重新加载 " << FACTS_FILE << "\n"; continue; }

        auto issues = detect_issues(input);
        cout << "检测到的问题：\n";
        if (issues.empty()) cout << "- 未检测到明显问题（仍建议使用证据和礼貌措辞）。\n";
        else for (auto &it : issues) cout << "- " << it << "\n";

        cout << "生成的回应候选：\n";
        auto responses = generate_responses(topic, input);
        for (size_t i=0;i<responses.size();++i) {
            cout << "[" << i+1 << "] 策略：" << responses[i].first << "\n";
            cout << responses[i].second << "\n\n";
        }

        cout << "操作：输入编号复制到剪贴板；输入 'save n' 将第 n 条保存为常用；输入 'list canned' 列表常用；输入空行继续。\n> ";
        string act;
        if (!getline(cin, act)) break;
        act = trim(act);
        if (act.empty()) continue;
        if (act == "list canned") {
            for (size_t i=0;i<CANNED.size();++i) cout << i+1 << ". " << CANNED[i] << "\n";
            continue;
        }
        if (act.rfind("save ",0)==0) {
            string num = trim(act.substr(5));
            int idx = stoi(num);
            if (idx>=1 && idx <= (int)responses.size()) {
                CANNED.push_back(responses[idx-1].second);
                save_canned();
                cout << "已保存到 " << CANNED_FILE << "\n";
            } else cout << "索引无效。\n";
            continue;
        }
        if (act.rfind("use ",0)==0) {
            string num = trim(act.substr(4));
            int idx = stoi(num);
            if (idx>=1 && idx <= (int)responses.size()) {
                bool ok = copy_to_clipboard(responses[idx-1].second);
                if (ok) cout << "已复制到剪贴板。\n";
                else cout << "复制失败，请手动复制：\n" << responses[idx-1].second << "\n";
            } else cout << "索引无效。\n";
            continue;
        }
        bool all_digits = !act.empty() && all_of(act.begin(), act.end(), [](char c){ return isdigit((unsigned char)c); });
        if (all_digits) {
            int idx = stoi(act);
            if (idx>=1 && idx <= (int)responses.size()) {
                bool ok = copy_to_clipboard(responses[idx-1].second);
                if (ok) cout << "已复制到剪贴板。\n";
                else cout << "复制失败，请手动复制：\n" << responses[idx-1].second << "\n";
            } else cout << "索引无效。\n";
            continue;
        }

        cout << "未识别操作，返回主循环。\n";
    }

    cout << "退出。已保存常用回应（如果有变动）。\n";
    save_canned();
    return 0;
}
