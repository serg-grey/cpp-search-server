#pragma once

#include <cstdint>
#include <iostream>
#include <vector>

#include "search_server.h"

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(const Iterator& range_begin, const Iterator& range_end)
        : it_begin_(range_begin), it_end_(range_end), size_(distance(it_begin_, it_end_))
    {
    }

    auto begin() const {
        return it_begin_;
    }
    auto end() const {
        return it_end_;
    }
    size_t size() const {
        return size_;
    }

private:
    Iterator it_begin_;
    Iterator it_end_;
    size_t size_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = std::min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_vector_.push_back({ begin, current_page_end });

            left -= current_page_size;
            begin = current_page_end;
        }
    }

    auto begin() const {
        return pages_vector_.begin();
    }

    auto end() const {
        return pages_vector_.end();
    }

    size_t size() const {
        return pages_vector_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_vector_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template<typename Iterator>
std::ostream& operator <<(std::ostream& out, const IteratorRange<Iterator>& pages)
{
    for (const auto& page : pages)
    {
        out << "{ "s
            << "document_id = "s << page.id << ", "s
            << "relevance = "s << page.relevance << ", "s
            << "rating = "s << page.rating << " }"s;
    }
    return out;
}