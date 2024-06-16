#pragma once

#include <iostream>
#include <format>

template <typename ...V>
void println(std::format_string<V...> const& fmt, V&&... v){
    std::cout << std::format(fmt, std::forward<V>(v)...) << std::endl;
}
template <typename ...V>
void println_err(std::format_string<V...> const& fmt, V&&... v){
    std::cerr << std::format(fmt, std::forward<V>(v)...) << std::endl;
}
