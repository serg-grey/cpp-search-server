#include "log_duration.h"
#include "paginator.h"
#include "request_queue.h"
#include "search_server.h"
#include "remove_duplicates.h"
#include "test_example_functions.h"


void AddDocument(SearchServer& search_server, int id, const string& doc, DocumentStatus status, vector<int> rating) {
    search_server.AddDocument(id, doc, status, rating);
}


int main() {
    setlocale(0, "");
    TestSearchServer();
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    std::cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << std::endl;
    RemoveDuplicates(search_server);
    std::cout << "After duplicates removed: "s << search_server.GetDocumentCount() << std::endl;



    int page_size = 2;
    RequestQueue request_queue(search_server);
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    const auto search_results1 = search_server.FindTopDocuments("curly rat"s);
    //все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly rat"s);
    const auto pages1 = Paginate(search_results1, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages1.begin(); page != pages1.end(); ++page) {
        std::cout << *page << std::endl;
        std::cout << "Page break"s << std::endl;
    }
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    return 0;
}