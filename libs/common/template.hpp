#include <algorithm>

template <typename T>
struct Vector2D {
    T x, y;

    template <typename U>
    Vector2D<T>& operator+=(const Vector2D<U>& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    template <typename U>
    Vector2D<T> operator+(const Vector2D<U>& rhs) const {
        auto tmp = *this;
        tmp += rhs;
        return tmp;
    }

    template <typename U>
    Vector2D<T>& operator-=(const Vector2D<U>& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    template <typename U>
    Vector2D<T> operator-(const Vector2D<U>& rhs) const {
        auto tmp = *this;
        tmp -= rhs;
        return tmp;
    }
};

template <typename T>
Vector2D<T> ElementMax(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
    return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y)};
}

template <typename T>
Vector2D<T> ElementMin(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
    return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y)};
}

template <typename T>
struct Rectangle {
    Vector2D<T> pos, size;
};

template <typename T, typename U>
Rectangle<T> operator&(const Rectangle<T>& lhs, const Rectangle<U>& rhs) {
    const auto lhs_end = lhs.pos + lhs.size;
    const auto rhs_end = rhs.pos + rhs.size;
    if (lhs_end.x < rhs.pos.x || lhs_end.y < rhs.pos.y ||
        rhs_end.x < lhs.pos.x || rhs_end.y < lhs.pos.y) {
        return {{0, 0}, {0, 0}};
    }

    auto new_pos = ElementMax(lhs.pos, rhs.pos);
    auto new_size = ElementMin(lhs_end, rhs_end) - new_pos;
    return {new_pos, new_size};
}