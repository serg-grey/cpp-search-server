#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
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
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        stop_words_ = MakeUniqueNonEmptyStrings(stop_words);
        for (const string& stop_word : stop_words_) {
            if (!IsValidWord(stop_word)) {
                throw invalid_argument("Недопустимоё стоп-слово, так как содержит спецсимволы"s);
            }  
        }
    }
    
    // Invoke delegating constructor from string container
    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) 
    {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Документ не был добавлен, так как его id отрицательный"s);
        }
        if (documents_.count(document_id) > 0) {
            throw invalid_argument("Документ не был добавлен, так как его id совпадает с уже имеющимся"s);
        }
        const vector<string>& words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Документ не был добавлен, так как содержит спецсимволы"s);
            }
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        index_table_.emplace_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
   
        vector<Document> matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < COMPARISON_ACCURACY_FOR_DOUBLE) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    int GetDocumentId(int index) const {
        return index_table_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        //обработка исключений будет внутри ParseQuery и ParseQueryWord
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
        return tuple{ matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector <int> index_table_;

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

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
        if (text[0] == '-') {
            if (text.size() == 1 || text[1] == '-') {
                throw invalid_argument("Ошибка в поисковом запросе (двойной минус или минус без слова)"s);
            }
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
                    if (!IsValidWord(query_word.data)) {
                        throw invalid_argument("Ошибка в поисковом запросе: минус-слово содержит содержит спецсимволы"s);
                    }
                    query.minus_words.insert(query_word.data);
                }
                else {
                    if (!IsValidWord(query_word.data)) {
                        throw invalid_argument("Ошибка в поисковом запросе: плюс-слово содержит содержит спецсимволы"s);
                    }
                    query.plus_words.insert(query_word.data);
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
        SearchServer search_server("is the"s);
        search_server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = search_server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer search_server("in the"s);
        //server.SetStopWords("in the"s);
        search_server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(search_server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer  search_server("in the"s);
    search_server.AddDocument(doc_id, "fluffy white cat with long tail"s, DocumentStatus::ACTUAL, ratings);
    {
        const string query = "fluffy white cat"s;
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    //тот же запрос, но с минус-словом
    {
        const string query = "fluffy white cat -tail"s;
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_HINT(search_server.FindTopDocuments(query).empty(),
            "Documents with minus-word must be excluded from results"s);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 42;
    const vector<int> ratings = { 1, 2, 3 };
    const string content = "white cat with long tail"s;
    SearchServer search_server("in the"s);
    search_server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        const string query = "fluffy white cat"s;
        const vector<string>expected_result = { "cat"s, "white"s };
        const vector<string>matched_words = get<vector<string>>(search_server.MatchDocument(query, doc_id));
        ASSERT_EQUAL(matched_words, expected_result);
    }
}

void TestSortingByRelevance() {
    SearchServer search_server("in the"s);
    const int doc0_id = 11;
    const int doc1_id = 12;
    const int doc2_id = 13;

    search_server.AddDocument(doc0_id, "funny fluffy fox"s, DocumentStatus::ACTUAL, { 3, 3, 3 }); //релевантность по 1 слову, больший рейтиннг
    search_server.AddDocument(doc1_id, "funny white cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 }); //наибольшая релевантность (2 слова)
    search_server.AddDocument(doc2_id, "fluffy grey dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 }); //релевантность по 1 слову, меньший рейтинг
    {
        const string query = "fluffy white cat"s;
        const vector<int>& expected_docs_order = { doc1_id ,doc0_id ,doc2_id };
        const vector< Document> found_docs = search_server.FindTopDocuments(query); //int id, double relevance, int rating = 0;
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
    const int expected_result = (1 + 2 + 3) / 3;
    {
        SearchServer search_server("in the"s);
        search_server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = search_server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, expected_result);
    }
}

void TestSearchWithUserPredicate() {
    SearchServer search_server("in the"s);
    //если результаты не найдены
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT(found_docs.empty());
    }
    search_server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, { 1,2,3 });
    search_server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::BANNED, { 8, -3 });
    search_server.AddDocument(12, "funny white cat"s, DocumentStatus::IRRELEVANT, { 3,3,3 });
    search_server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::REMOVED, { -1,3,4 });
    //предикат с условием по id
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL(doc0.id, 10); //??12
        ASSERT_EQUAL(doc1.id, 12); //??10
    }
    //предикат с условием по статусу
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 11);
    }
    //предикат с условием по рейтингу
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 2; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 12);
    }
}

void TestSearchWithCurrentStatus() {
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer search_server("in the"s);
    search_server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::BANNED, ratings);
    search_server.AddDocument(12, "funny white cat"s, DocumentStatus::IRRELEVANT, ratings);
    search_server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::REMOVED, ratings);
    //по умолчанию должны выдаваться документы со статусом DocumentStatus::ACTUAL
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 10);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 10);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 11);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 12);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("fluffy cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 13);
    }
}

void TestRelevanceComputing() {
    const string stop_words = "is are was a an in the with near at"s;
    SearchServer search_server(stop_words);
    const double COMPARISON_ACCURACY = (1e-6);
    const int doc_count = 3;
    const int doc0_id = 11;
    const int doc1_id = 12;
    const int doc2_id = 13;
    const string doc0_content = "a colorful parrot with green wings and red tail is lost"s;
    const string doc1_content = "a grey hound with black ears is found at the railway station"s;
    const string doc2_content = "a white cat with long furry tail is found near the red square"s;
    const string query = "white cat long tail"s;
    const set<string> query_words = { "white"s, "cat"s, "long"s, "tail"s };
    const map<int, vector<string>>& docs = { {doc0_id, { "colorful"s, "parrot"s, "green"s, "wings"s, "and"s, "red"s, "tail"s, "lost"s } },
                                            {doc1_id, { "grey"s, "hound"s, "black"s, "ears"s, "found"s, "railway"s, "station"s }},
                                            {doc2_id, { "white"s, "cat"s, "long"s, "furry"s, "tail"s, "found"s, "red"s, "square"s }}
    };
    map<string, map<int, double>> word_to_document_freqs;// слово запроса, <id документа, частота слова в нём>

    for (const auto& [id, doc] : docs) {
        const double inv_word_count = 1.0 / doc.size();
        for (const string& word : doc) {
            word_to_document_freqs[word][id] += inv_word_count;
        }
    }

    map<int, double> document_to_relevance;

    for (const string& word : query_words) {
        if (word_to_document_freqs.count(word) == 0) {
            continue;
        }
        const double idf = log(doc_count * 1.0 / word_to_document_freqs.at(word).size());
        for (const auto [doc_id, tf] : word_to_document_freqs.at(word)) {
            document_to_relevance[doc_id] += tf * idf;
        }
    }

    const vector<int>& expected_docs_order = { doc2_id, doc0_id };              //ожидаемый порядок найденных документов
    const double doc0_expected_relevance = document_to_relevance.at(doc2_id);   //ожидаемая релевантность для doc2 ~0.462663
    const double doc2_expected_relevance = document_to_relevance.at(doc0_id);   //ожидаемая релевантность для doc0 ~0.0506831
    const vector<int> ratings = { 1, 2, 3 };

    search_server.AddDocument(doc0_id, doc0_content, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(doc1_id, doc1_content, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(doc2_id, doc2_content, DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const vector<int>& found_docs_order = { doc0.id, doc1.id };
        ASSERT_EQUAL_HINT(found_docs_order, expected_docs_order,
            "Wrong order of documents. Documents should be sorted by relevance. Documents with equal relevance should be sorted by rating."s);
        ASSERT_HINT(abs(doc0.relevance - doc0_expected_relevance) < COMPARISON_ACCURACY, "incorrect result of relevance calculation."s);
        ASSERT_HINT(abs(doc1.relevance - doc2_expected_relevance) < COMPARISON_ACCURACY, "incorrect result of relevance calculation."s);
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
    cout << endl;
}

// --------- Окончание модульных тестов поисковой системы -----------


// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    setlocale(0, "");
    TestSearchServer();
    SearchServer search_server("и в на"s);
    try {
        SearchServer wrong_search_server("и в на\x12"s);
    }
    catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        (void)search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    } catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    } catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    }
    catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    }
    catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        const auto documents = search_server.FindTopDocuments("--пушистый"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }
    catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        const auto documents = search_server.FindTopDocuments("к\x13от -пуши\x14тый"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    }
    catch (invalid_argument& error) {
        cout << error.what() << endl;
    }
    try {
        search_server.GetDocumentId(-5);
    }
    catch (out_of_range& error) {
        cout << error.what() << " Введён несуществующий индекс документа."s << endl;
    }

    return 0;
}