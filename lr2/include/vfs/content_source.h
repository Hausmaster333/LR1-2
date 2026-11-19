#ifndef CONTENT_SOURCE_H
#define CONTENT_SOURCE_H

#include <cstddef>
#include <string>

// Откуда файл берёт содержимое: реальный файл на диске или генератор тестовых данных.
// В семестровой задаче добавятся секторы в адресном пространстве.
class IContentSource {
    public:
        virtual long long get_size() const = 0; // Размер в байтах, -1 если данные недоступны
        virtual std::string read_beginning(std::size_t max_bytes) const = 0; // Первые байты
        virtual std::string get_location() const = 0; // Где лежат данные, для показа пользователю
        virtual ~IContentSource() = default;
};

// Реальный файл: виртуальная ФС только читает его и никогда не меняет
class RealFileSource : public IContentSource {
    private:
        std::string real_path; // Абсолютный путь в UTF-8
    public:
        explicit RealFileSource(const std::string& real_path);

        long long get_size() const override;
        std::string read_beginning(std::size_t max_bytes) const override;
        std::string get_location() const override;
};

// Пустой файл от генератора тестовых данных и бенчмарка: диск не трогается
class GeneratedSource : public IContentSource {
    public:
        long long get_size() const override { return 0; }
        std::string read_beginning(std::size_t) const override { return ""; }
        std::string get_location() const override { return "сгенерирован, без данных"; }
};

#endif
