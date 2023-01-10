﻿#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double COMPARISON_ACCURACY_FOR_DOUBLE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id = 0;
    double relevance = 0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.emplace(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    // document_predicate = [](int document_id, DocumentStatus status, int rating + тело функции из вызова
    // например [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })

    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < COMPARISON_ACCURACY_FOR_DOUBLE) {
                    return lhs.rating > rhs.rating;
                }
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus current_status) const {
        //если в запросе есть строка и статус, фильтрация по сравнению статусов
        auto matched_documents = FindTopDocuments(raw_query, [current_status](int document_id, DocumentStatus status, int rating) {
            return status == current_status;
            });
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        // если в запросе только строка, фильтрация будет по дефолтному уловию DocumentStatus::ACTUAL
        auto matched_documents = FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        return matched_documents;
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.emplace(query_word.data);
                }
                else {
                    query.plus_words.emplace(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>

    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    // если условие из main выполняется для данного документа
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// -------- шаблонные функции и макросы для тестов ----------

template <typename T>
ostream& operator<<(ostream& os, const set<T>& container) {
    bool is_first = true;
    os << "{"s;
    for (const auto& element : container) {
        if (!is_first) {
            os << ", "s;
        }
        os << element;
        is_first = false;
    }
    os << "}"s;
    return os;
}

template <typename T, typename U>
ostream& operator<<(ostream& os, const map<T, U>& container) {
    bool is_first = true;
    os << "{"s;
    for (const auto& [key, value] : container) {
        if (!is_first) {
            os << ", "s;
        }
        os << key << ": "s << value;
        is_first = false;
    }
    os << "}"s;
    return os;
}

template <typename T>
ostream& operator<<(ostream& os, const vector<T>& container) {
    bool is_first = true;
    os << "["s;
    for (const auto& element : container) {
        if (!is_first) {
            os << ", "s;
        }
        os << element;
        is_first = false;
    }
    os << "]"s;
    return os;
}


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void AssertImpl(const T& t, const string& t_str, const string& file,
    const string& func, unsigned line, const string& hint) {

    if (t != true) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << t_str << ") failed. "s;
        if (!hint.empty()) {
            cout << "Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& test, const string& test_name) {
    test();
    cerr << test_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, "fluffy white cat with long tail"s, DocumentStatus::ACTUAL, ratings);
    {
        const string query = "fluffy white cat"s;
        const auto found_docs = server.FindTopDocuments(query);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    //тот же запрос, но с минус-словом
    {
        const string query = "fluffy white cat -tail"s;
        const auto found_docs = server.FindTopDocuments(query);
        ASSERT_HINT(server.FindTopDocuments(query).empty(),
            "Documents with minus-word must be excluded from results"s);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 42;
    const vector<int> ratings = { 1, 2, 3 };
    const string content = "white cat with long tail"s;
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        const string query = "fluffy white cat"s;
        const vector<string>expected_result = { "cat"s, "white"s };
        const vector<string>matched_words = get<vector<string>>(server.MatchDocument(query, doc_id));
        ASSERT_EQUAL(matched_words, expected_result);
    }
}

void TestSortingByRelevance() {
    SearchServer server;
    const int doc0_id = 11;
    const int doc1_id = 12;
    const int doc2_id = 13;
    server.AddDocument(doc0_id, "funny white cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 }); //наибольшая релевантность (2 слова)
    server.AddDocument(doc1_id, "funny fluffy fox"s, DocumentStatus::ACTUAL, { 3, 3, 3 }); //релевантность по 1 слову, больший рейтиннг
    server.AddDocument(doc2_id, "fluffy grey dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 }); //релевантность по 1 слову, меньший рейтинг
    {
        const string query = "fluffy white cat"s;
        const vector<int>& expected_docs_order = { doc0_id ,doc1_id ,doc2_id };
        const vector< Document> found_docs = server.FindTopDocuments(query); //int id, double relevance, int rating = 0;
        ASSERT_EQUAL_HINT(found_docs.size(), 3u,
            "Wrong number of documents found."s);
        const vector<int>& found_docs_order = { found_docs[0].id, found_docs[1].id, found_docs[2].id };
        ASSERT_EQUAL_HINT(expected_docs_order, found_docs_order,
            "Wrong order of documents. Documents should be sorted by relevance. Documents with equal relevance should be sorted by rating."s);
    }
}

void TestDocumentRatingComputing() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    const int expected_result = 2;//(1+2+3)/3
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, expected_result);
    }
}

void TestSearchWithUserPredicate() {
    SearchServer server;
    //если результаты не найдены
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT(found_docs.empty());
    }
    server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::BANNED, { 8, -3 });
    server.AddDocument(12, "funny white cat"s, DocumentStatus::IRRELEVANT, { 3,3,3 });
    server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::REMOVED, { -1,3,4 });
    //предикат с условием по id
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, 12);
        ASSERT_EQUAL(doc1.id, 10);
    }
    //предикат с условием по статусу
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 11);
    }
    //предикат с условием по рейтингу
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 2; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 12);
    }
}

void TestSearchWithCurrentStatus() {
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::BANNED, ratings);
    server.AddDocument(12, "funny white cat"s, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::REMOVED, ratings);
    //по умолчанию должны выдаваться документы со статусом DocumentStatus::ACTUAL
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 10);
    }
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 10);
    }
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 11);
    }
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 12);
    }
    {
        const auto found_docs = server.FindTopDocuments("fluffy cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 13);
    }
}

void TestRelevanceComputing() {
    const double COMPARISON_ACCURACY_FOR_DOUBLE = 1e-6;
    const int doc0_id = 11;
    const int doc1_id = 12;
    const int doc2_id = 13;
    const vector<int>& expected_docs_order = { doc2_id, doc0_id };
    const double doc0_expected_relevance = 0.462663;
    const double doc2_expected_relevance = 0.0506831;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;

    server.SetStopWords("is are was a an in the with near at"s);
    server.AddDocument(doc0_id, "a colorful parrot with green wings and red tail is lost"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc1_id, "a grey hound with black ears is found at the railway station"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc2_id, "a white cat with long furry tail is found near the red square"s, DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = server.FindTopDocuments("white cat long tail"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const vector<int>& found_docs_order = { doc0.id, doc1.id };
        ASSERT_EQUAL_HINT(found_docs_order, expected_docs_order,
            "Wrong order of documents. Documents should be sorted by relevance. Documents with equal relevance should be sorted by rating."s);
        ASSERT_HINT(abs(doc0.relevance - doc0_expected_relevance) < COMPARISON_ACCURACY_FOR_DOUBLE, "incorrect result of relevance calculation."s);
        ASSERT_HINT(abs(doc1.relevance - doc2_expected_relevance) < COMPARISON_ACCURACY_FOR_DOUBLE, "incorrect result of relevance calculation."s);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestDocumentRatingComputing);
    RUN_TEST(TestSearchWithUserPredicate);
    RUN_TEST(TestSearchWithCurrentStatus);
    RUN_TEST(TestRelevanceComputing);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
