template <class Key>
const Index<Key>& VirtualFileSystem::get_or_build_index(SearchCriterion<ShrdPtr<const File>, Key> criterion, bool rebuild) {
    for (auto& entry : indexes) {
        auto* cached = std::any_cast<CachedIndex<Key>>(&entry);
        if (cached && cached->criterion.extract == criterion.extract) {
            if (rebuild) cached->index = ::build_index(get_or_collect_files(), criterion);
            return cached->index;
        }
    }
    auto& entry = indexes.push_back(CachedIndex<Key>{criterion, ::build_index(get_or_collect_files(), criterion)});
    return std::any_cast<const CachedIndex<Key>&>(entry).index;
}

template <class Key>
DynamicArray<ShrdPtr<const File>> VirtualFileSystem::find_files(SearchCriterion<ShrdPtr<const File>, Key> criterion,
        const Key& key, SearchMethod method) {
    if (method == SearchMethod::Index) {
        auto positions = get_or_build_index(criterion).find(key);
        const auto& files = *indexed_files;
        DynamicArray<ShrdPtr<const File>> matches(positions.get_size());
        for (std::size_t i = 0; i < positions.get_size(); ++i)
            matches.set(i, files[positions.get(i)]);
        return matches;
    }
    ArraySequence<ShrdPtr<const File>> matches;
    visit_files(root, [&](const ShrdPtr<const File>& file) {
        if (file == root) return;
        auto value = criterion.extract(file);
        if (value && !(*value < key) && !(key < *value)) matches.push_back(file);
    });
    return DynamicArray<ShrdPtr<const File>>(matches.data(), matches.size());
}
