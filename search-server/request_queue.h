#pragma once

#include <deque>
#include <string>
#include <vector>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_(search_server) {}

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        auto request = search_server_.FindTopDocuments(raw_query, document_predicate);
        if (requests_.size() >= min_in_day_) {
            requests_.pop_front();
            if (query_results_.no_result_requests_ > 0) {
                --query_results_.no_result_requests_;
            }
        }
        if (request.empty()) {
            ++query_results_.no_result_requests_;
        }
        requests_.push_front({ static_cast<int>(requests_.size()) });
        return request;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    const SearchServer& search_server_;
    struct QueryResult {
        int successful_requests_ = 0;
        int no_result_requests_ = 0;

    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    QueryResult query_results_;
};