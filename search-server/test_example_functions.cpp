
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "paginator.h"
#include "remove_duplicates.h"
#include "request_queue.h"
#include "string_processing.h"
#include "test_example_functions.h"

// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::vector<int> ratings = { 1, 2, 3 };
    SearchServer  search_server("in the"s);
    search_server.AddDocument(doc_id, "fluffy white cat with long tail"s, DocumentStatus::ACTUAL, ratings);
    {
        const std::string query = "fluffy white cat"s;
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    //тот же запрос, но с минус-словом
    {
        const std::string query = "fluffy white cat -tail"s;
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_HINT(search_server.FindTopDocuments(query).empty(),
            "Documents with minus-word must be excluded from results"s);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 42;
    const std::vector<int> ratings = { 1, 2, 3 };
    const std::string content = "white cat with long tail"s;
    SearchServer search_server("in the"s);
    search_server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    {
        const std::string query = "fluffy white cat"s;
        const std::vector<std::string>expected_result = { "cat"s, "white"s };
        const std::vector<std::string>matched_words = std::get<std::vector<std::string>>(search_server.MatchDocument(query, doc_id));
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
        const std::string query = "fluffy white cat"s;
        const std::vector<int>& expected_docs_order = { doc1_id ,doc0_id ,doc2_id };
        const std::vector< Document> found_docs = search_server.FindTopDocuments(query); //int id, double relevance, int rating = 0;
        ASSERT_EQUAL_HINT(found_docs.size(), 3u,
            "Wrong number of documents found."s);
        const std::vector<int>& found_docs_order = { found_docs[0].id, found_docs[1].id, found_docs[2].id };
        ASSERT_EQUAL_HINT(expected_docs_order, found_docs_order,
            "Wrong order of documents. Documents should be sorted by relevance. Documents with equal relevance should be sorted by rating."s);
    }
}

void TestDocumentRatingComputing() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
        ASSERT_EQUAL(doc0.id, 10);
        ASSERT_EQUAL(doc1.id, 12);
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
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string stop_words = "is are was a an in the with near at"s;
    SearchServer search_server(stop_words);
    const double COMPARISON_ACCURACY = (1e-6);
    const int doc_count = 3;
    const int doc0_id = 11;
    const int doc1_id = 12;
    const int doc2_id = 13;
    const std::string doc0_content = "a colorful parrot with green wings and red tail is lost"s;
    const std::string doc1_content = "a grey hound with black ears is found at the railway station"s;
    const std::string doc2_content = "a white cat with long furry tail is found near the red square"s;
    const std::string query = "white cat long tail"s;
    const std::set<std::string> query_words = { "white"s, "cat"s, "long"s, "tail"s };
    const std::map<int, std::vector<std::string>>& docs = { {doc0_id, { "colorful"s, "parrot"s, "green"s, "wings"s, "and"s, "red"s, "tail"s, "lost"s } },
                                            {doc1_id, { "grey"s, "hound"s, "black"s, "ears"s, "found"s, "railway"s, "station"s }},
                                            {doc2_id, { "white"s, "cat"s, "long"s, "furry"s, "tail"s, "found"s, "red"s, "square"s }}
    };
    std::map<std::string, std::map<int, double>> word_to_document_freqs;// слово запроса, <id документа, частота слова в нём>

    for (const auto& [id, doc] : docs) {
        const double inv_word_count = 1.0 / doc.size();
        for (const std::string& word : doc) {
            word_to_document_freqs[word][id] += inv_word_count;
        }
    }

    std::map<int, double> document_to_relevance;

    for (const std::string& word : query_words) {
        if (word_to_document_freqs.count(word) == 0) {
            continue;
        }
        const double idf = std::log(doc_count * 1.0 / word_to_document_freqs.at(word).size());
        for (const auto [doc_id, tf] : word_to_document_freqs.at(word)) {
            document_to_relevance[doc_id] += tf * idf;
        }
    }

    const std::vector<int>& expected_docs_order = { doc2_id, doc0_id };              //ожидаемый порядок найденных документов
    const double doc0_expected_relevance = document_to_relevance.at(doc2_id);   //ожидаемая релевантность для doc2 ~0.462663
    const double doc2_expected_relevance = document_to_relevance.at(doc0_id);   //ожидаемая релевантность для doc0 ~0.0506831
    const std::vector<int> ratings = { 1, 2, 3 };

    search_server.AddDocument(doc0_id, doc0_content, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(doc1_id, doc1_content, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(doc2_id, doc2_content, DocumentStatus::ACTUAL, ratings);

    {
        const auto found_docs = search_server.FindTopDocuments(query);
        ASSERT_EQUAL_HINT(found_docs.size(), 2u,
            "Wrong number of documents found."s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const std::vector<int>& found_docs_order = { doc0.id, doc1.id };
        ASSERT_EQUAL_HINT(found_docs_order, expected_docs_order,
            "Wrong order of documents. Documents should be sorted by relevance. Documents with equal relevance should be sorted by rating."s);
        ASSERT_HINT(abs(doc0.relevance - doc0_expected_relevance) < COMPARISON_ACCURACY, "incorrect result of relevance calculation."s);
        ASSERT_HINT(abs(doc1.relevance - doc2_expected_relevance) < COMPARISON_ACCURACY, "incorrect result of relevance calculation."s);
    }
}

void TestPaginator() {
    const std::vector<int> ratings = { 1, 2, 3 };
    SearchServer search_server("in the"s);
    search_server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(12, "funny white cat"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::ACTUAL, ratings);

    const auto search_results = search_server.FindTopDocuments("funny cat"s);

    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);

    ASSERT_HINT(distance(pages.begin(), pages.end()) == page_size, "incorrect result page size."s);

    std::vector<Document> results;
    for (auto& page : pages) {
        results.emplace_back(*page.begin());
        if (distance(page.begin(), page.end()) == page_size) {
            results.emplace_back(*(page.begin() + 1));
        }
    }

    ASSERT_EQUAL(results.at(0).id, search_results.at(0).id);
    ASSERT_EQUAL(results.at(1).id, search_results.at(1).id);
    ASSERT_EQUAL(results.at(2).id, search_results.at(2).id);

}

void Test_RequestQueue() {
    const std::vector<int> ratings = { 1, 2, 3 };
    SearchServer search_server("in the"s);

    RequestQueue request_queue(search_server);

    const int empty_requests = 1439;
    for (int i = 0; i < empty_requests; ++i) {
        request_queue.RequestQueue::AddFindRequest("empty request"s);
    }

    ASSERT_EQUAL_HINT(request_queue.RequestQueue::GetNoResultRequests(), empty_requests, "Wrong number of empty requests at start"s);

    search_server.AddDocument(10, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(11, "fluffy grey dog"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(12, "funny white cat"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(13, "funny fluffy fox"s, DocumentStatus::ACTUAL, ratings);

    request_queue.AddFindRequest("funny cat"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests, "Wrong number of empty requests after first right query"s);

    request_queue.AddFindRequest("grey fox"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests - 1, "Wrong number of empty requests after right query"s);

    request_queue.AddFindRequest("green parrot"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests - 1, "Wrong number of empty requests after wrong query"s);

    request_queue.AddFindRequest("white cat"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests - 2, "Wrong number of empty requests after right query"s);
}

void Test_RemoveDuplicates() {
    SearchServer search_server("and with"s);
    const std::vector<int> ratings = { 1, 2, 3 };

    search_server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(2, "fluffy grey dog"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(3, "fluffy grey dog"s, DocumentStatus::ACTUAL, ratings);// дубликат документа 2
    search_server.AddDocument(4, "funny white cat"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(5, "cat in the city"s, DocumentStatus::ACTUAL, ratings);// дубликат документа 1
    search_server.AddDocument(6, "fluffy grey dog"s, DocumentStatus::ACTUAL, ratings);// дубликат документа 2
    search_server.AddDocument(7, "funny fluffy fox"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(8, "cat in the city"s, DocumentStatus::ACTUAL, ratings);// дубликат документа 1

    ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 8, "Wrong number of documents before removing duplicates"s);
    RemoveDuplicates(search_server);
    ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 4, "Wrong number of documents after removing duplicates"s);
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
    RUN_TEST(TestPaginator);
    RUN_TEST(Test_RequestQueue);
    RUN_TEST(Test_RemoveDuplicates);

    std::cout << std::endl;
}

// --------- Окончание модульных тестов поисковой системы -----------
