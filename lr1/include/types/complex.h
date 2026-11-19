#ifndef COMPLEX_H
#define COMPLEX_H

#include <cmath>

class Complex {
    private:
        double real;
        double imag;
    public:
        Complex() : real(0.0), imag(0.0) {}
        Complex(double real, double imag) : real(real), imag(imag) {}

        double get_real() const { return real; }
        double get_imag() const { return imag; }
        double modulus() const { return std::sqrt(real * real + imag * imag); }

        bool operator==(const Complex& other) const {
            return real == other.real && imag == other.imag;
        }
        bool operator!=(const Complex& other) const { return !(*this == other); }

        // Сначала по модулю, при равных модулях по (real, imag): полный порядок для BST,
        // иначе 1+0i и 0+1i считались бы одним элементом
        bool operator<(const Complex& other) const {
            double this_modulus = modulus();
            double other_modulus = other.modulus();
            if (this_modulus != other_modulus) return this_modulus < other_modulus;
            if (real != other.real) return real < other.real;
            return imag < other.imag;
        }
        bool operator>(const Complex& other) const { return other < *this; }
};

#endif
