#include "paginator.h"
#include "request_queue.h"
#include "search_server.h"
#include "test_example_functions.h"

int main() {
    setlocale(0, "");
    TestSearchServer();
    SearchServer search_server("and with"s);

    int page_size = 2;
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    const auto search_results1 = search_server.FindTopDocuments("curly dog"s);
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    const auto pages1 = Paginate(search_results1, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages1.begin(); page != pages1.end(); ++page) {
        std::cout << *page << std::endl;
        std::cout << "Page break"s << std::endl;
    }
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    const auto search_results2 = search_server.FindTopDocuments("big collar"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    const auto pages2 = Paginate(search_results2, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages2.begin(); page != pages2.end(); ++page) {
        std::cout << *page << std::endl;
        std::cout << "Page break"s << std::endl;
    }
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    const auto search_results3 = search_server.FindTopDocuments("sparrow"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    const auto pages3 = Paginate(search_results3, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages3.begin(); page != pages3.end(); ++page) {
        std::cout << *page << std::endl;
        std::cout << "Page break"s << std::endl;
    }
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    return 0;
}