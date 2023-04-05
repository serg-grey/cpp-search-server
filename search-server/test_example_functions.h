#pragma once

#include <cassert>
#include <iostream>

#include "search_server.h"
#include "paginator.h"
#include"request_queue.h"

// -------- шаблонные функции и макросы для тестов ----------

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& container) {
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
std::ostream& operator<<(std::ostream& os, const std::map<T, U>& container) {
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
std::ostream& operator<<(std::ostream& os, const std::vector<T>& container) {
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
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void AssertImpl(const T& t, const std::string& t_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {

    if (t != true) {
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT("s << t_str << ") failed. "s;
        if (!hint.empty()) {
            std::cout << "Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& test, const std::string& test_name) {
    test();
    std::cerr << test_name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent();
void TestExcludeDocumentsWithMinusWords();
void TestMatchingDocuments();
void TestSortingByRelevance();
void TestDocumentRatingComputing();
void TestSearchWithUserPredicate();
void TestSearchWithCurrentStatus();
void TestRelevanceComputing();
void TestPaginator();
void TestRequestQueue();

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -----------
