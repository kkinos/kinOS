# C\C++

## テンプレートの特殊化　 p287

```c++
template <typename T>
    uint8_t SumBytes(const T* data, size_t bytes) {
        return SumBytes(reinterpret_cast<const uint8_t*>(data), bytes);
    }

template <>
    uint8_t SumBytes<uint8_t>(const uint8_t* data, size_t bytes) {
        uint8_t sum = 0;
        for (size_t i = 0; i < bytes; ++i) {
            sum += data[i];
        }
        return sum;
    }
```

上の template は一般的な定義で、下の template は T が uint8_t のときの処理。これにより関数を呼び出す側はキャストする必要がなくなる
