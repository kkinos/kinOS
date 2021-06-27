# C\C++

## 動的にメモリを確保する

```c++
int n;
int *a_heap;

std::cin >> n; // nを入力

a_heap = new int[n]; // new演算子を使うことで定数でなくとも配列を定義できる

// 何かしらの処理

delete[] a_heap; // 確保した領域は削除
```

スタック領域とヒープ領域があり、ヒープ領域はプログラムの中で領域を動的に確保できる。つまり new というのはヒープ領域で確保する領域の先頭アドレスをスタック領域に入れておくもの？

[参考 1](https://brain.cc.kogakuin.ac.jp/~kanamaru/lecture/prog1/11-02.html)
[参考 2](https://www.uquest.co.jp/embedded/learning/lecture16.html)

## 共用体

```c++
union UIntInByte
{
    int           i;
    unsigned char c[4];
};
```

i と c はメモリの同じ位置に存在する。つまり先頭アドレスが同じでメモリを共用している。共用体を使えば同じメモリ領域に違う名前を付けて、さらに違う型で扱うことができる。

## 関数ポインタ

```c++
int add(int a, int b) {
  return (a + b);
}

int (*func)(int, int) = add; // 戻り値の型 (*名前)(引数)

```

関数のアドレスは()を省いたもの。上記でいうと`add`

## 演算子のオーバーロード p137

```c++
template <typename T>
struct Vector2D {
  T x, y;

  template <typename U>
  Vector2D<T>& operator +=(const Vector2D<U>& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
};
```

自作クラスのインスタンス同士の演算を定義するときに演算子のオーバーロードを用いる。上記の例では`Vector2D<int> pos1 += Vector2D<int> pos2`ということができる。

## 構造体の各フィードを詰める p167

```c++
struct InterruptDescriptor {
  uint16_t offset_low;
  uint16_t segment_selector;
  InterruptDescriptorAttribute attr;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));
```

コンパイラは何もしていないと記憶装置にデータを書き込むとき、パディングを挿入してデータの大きさや書き込む位置を調整してしまう。しかしハードウェアの仕様で定まったデータ構造を構造体として表現するときには隙間を入れられるのは困るので`__attribute__((packed))`をつける必要がある。

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
