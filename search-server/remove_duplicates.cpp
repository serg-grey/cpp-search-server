#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string>, int> words_to_id;
    std::set<int> duplicate_ids;
    for (const int document_id : search_server) {
        std::map<std::string, double> word_to_id_freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string> words;
        for (const auto& [word, freq] : word_to_id_freqs) {
            words.emplace(word);
        }
        if (words_to_id.count(words)) {
            duplicate_ids.emplace(document_id);
        }
        else {
            words_to_id[words] = document_id;
        }
    }

    for (const int id : duplicate_ids) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id " << id << std::endl;
    }
}