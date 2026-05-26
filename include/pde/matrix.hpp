#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace pde {

using Word = std::uint32_t;

class Matrix {
public:
    Matrix() = default;

    Matrix(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols), data_(checked_size(rows, cols)) {}

    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept { return cols_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    [[nodiscard]] Word* row_data(std::size_t row) noexcept {
        return data_.data() + row * cols_;
    }

    [[nodiscard]] const Word* row_data(std::size_t row) const noexcept {
        return data_.data() + row * cols_;
    }

    [[nodiscard]] Word& operator()(std::size_t row, std::size_t col) noexcept {
        return data_[row * cols_ + col];
    }

    [[nodiscard]] const Word& operator()(std::size_t row, std::size_t col) const noexcept {
        return data_[row * cols_ + col];
    }

    [[nodiscard]] const std::vector<Word>& data() const noexcept { return data_; }
    [[nodiscard]] std::vector<Word>& data() noexcept { return data_; }

private:
    static std::size_t checked_size(std::size_t rows, std::size_t cols) {
        if (rows == 0 || cols == 0) {
            throw std::invalid_argument("matrix dimensions must be non-zero");
        }
        if (rows > static_cast<std::size_t>(-1) / cols) {
            throw std::overflow_error("matrix dimensions overflow size_t");
        }
        return rows * cols;
    }

    std::size_t rows_ = 0;
    std::size_t cols_ = 0;
    std::vector<Word> data_;
};

[[nodiscard]] inline bool same_shape(const Matrix& lhs, const Matrix& rhs) noexcept {
    return lhs.rows() == rhs.rows() && lhs.cols() == rhs.cols();
}

[[nodiscard]] inline bool equal(const Matrix& lhs, const Matrix& rhs) noexcept {
    return same_shape(lhs, rhs) && lhs.data() == rhs.data();
}

}  // namespace pde
