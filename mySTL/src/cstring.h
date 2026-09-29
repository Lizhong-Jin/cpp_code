#pragma once

#ifndef CSTRING_H
#define CSTRING_H

#include <cstddef>

namespace mystl {
    // ******************************************************************
    // memory primitives
    constexpr void* memcpy(void* dest, const void* src, const std::size_t n) noexcept {
        auto* d=static_cast<unsigned char*>(dest);
        const auto* s=static_cast<const unsigned char*>(src);
        for(std::size_t i=0; i<n; ++i) d[i]=s[i];
        return dest;
    };

    constexpr void* memmove(void* dest, const void* src, const std::size_t n) noexcept {
        auto* d=static_cast<unsigned char*>(dest);
        const auto* s=static_cast<const unsigned char*>(src);
        if (n==0 || d==s) return dest;
        if (d<s || d>s+n) {
            for(std::size_t i=0; i<n; ++i) d[i]=s[i];
        }else {
            for(std::size_t i=n; i!=0;) {--i; d[i]=s[i];}
        }
        return dest;
    }

    constexpr void* memset(void* s, const int c, std::size_t n) noexcept {
        auto* d=static_cast<unsigned char*>(s);
        auto p=static_cast<unsigned char>(c);
        for(std::size_t i=0; i<n; ++i) d[i]=p;
        return s;
    }

    constexpr int memcmp(const void* s1, const void* s2, std::size_t n) noexcept {
        const auto* d1=static_cast<const unsigned char*>(s1);
        const auto* d2=static_cast<const unsigned char*>(s2);
        for(std::size_t i=0; i<n; ++i) if(d1[i]!=d2[i]) return (int)(d1[i]-d2[i]);
        return 0;
    }

    constexpr void* memchr(const void* s, const int c, std::size_t n) noexcept {
        const auto* d=static_cast<const unsigned char*>(s);
        auto p=static_cast<unsigned char>(c);
        for(std::size_t i=0; i<n; ++i) if(d[i]==p) return const_cast<unsigned char*>(d+i);
        return nullptr;
    }

    // ******************************************************************
    // C-string primitives
    constexpr std::size_t strlen(const char* s) noexcept {
        std::size_t n=0; while(s[n]!='\0') ++n; return n;
    }

    constexpr int strcmp(const char* s1, const char* s2) noexcept {
        while (*s1 && *s1==*s2) ++s1, ++s2;
        return (int)(static_cast<unsigned char>(*s1)-static_cast<unsigned char>(*s2));
    }

    constexpr int strncmp(const char* s1, const char* s2, std::size_t n) noexcept {
        for(std::size_t i=0; i<n; ++i) {
            const auto c1=static_cast<unsigned char>(s1[i]);
            if(const auto c2=static_cast<unsigned char>(s2[i]); c1!=c2) return static_cast<int>(c1) - static_cast<int>(c2);
            if(c1==0) return 0;
        }
        return 0;
    }

    constexpr char* strcpy(char* dest, const char* src) noexcept {
        char* d=dest;
        while ((*d++=*src++) != '\0') {}
        return dest;
    }

    constexpr char* strncpy(char* dest, const char* src, std::size_t n) noexcept {
        std::size_t i=0;
        while (i<n && src[i] != '\0') {dest[i]=src[i]; ++i;}
        while (i<n) {dest[i]='\0'; ++i;}
        return dest;
    }

    constexpr char* strcat(char* dest, const char* src) noexcept {
        char* d=dest; while (*d) ++d;
        while ((*d++=*src++) != '\0') {}
        return dest;
    }

    constexpr char* strncat(char* dest, const char* src, std::size_t n) noexcept {
        char* d=dest; while (*d) ++d;
        std::size_t i=0;
        while (i<n && src[i] != '\0') {d[i]=src[i]; ++i;}  d[i]='\0';
        return dest;
    }

    // ******************************************************************
    namespace detail {
        template <typename CharPtr>
        constexpr CharPtr strchr_impl(CharPtr s, int c) noexcept {
            const auto ch=static_cast<unsigned char>(c);
            while (*s) {
                if (static_cast<unsigned char>(*s)==ch) return s;
                ++s;
            }
            return ch == 0 ? s : nullptr;
        }
        template <typename CharPtr>
        constexpr CharPtr strrchr_impl(CharPtr s, int c) noexcept {
            const auto ch=static_cast<unsigned char>(c);
            CharPtr last=nullptr;
            while (*s) {
                if (static_cast<unsigned char>(*s)==ch) last=s;
                ++s;
            }
            if (ch == 0) return s;
            return last;
        }
        template <typename CharPtr>
        constexpr CharPtr strstr_impl(CharPtr s, const char* n) noexcept {
            if (*n == '\0') return s;
            while (*s) {
                CharPtr s_t=s;
                const char* n_t=n;
                while (*s_t && * n_t && *s_t == *n_t) {++s_t; ++n_t;}
                if (*n_t == 0) return s;
                ++s;
            }
            return nullptr;
        }
    } // namespace detail
    // ******************************************************************

    constexpr char* strchr(char* s, int c) noexcept {return detail::strchr_impl<char*>(s, c);}
    constexpr const char* strchr(const char* s, int c) noexcept {return detail::strchr_impl<const char*>(s, c);}

    constexpr char* strrchr(char* s, int c) noexcept {return detail::strrchr_impl<char*>(s, c);}
    constexpr const char* strrchr(const char* s, int c) noexcept {return detail::strrchr_impl<const char*>(s, c);}

    constexpr char* strstr(char* s, const char* n) noexcept {return detail::strstr_impl<char*>(s, n);}
    constexpr const char* strstr(const char* s, const char* n) noexcept {return detail::strstr_impl<const char*>(s, n);}

    constexpr std::size_t strspn(const char* s, const char* accept) noexcept {
        bool table[256]={false};
        for (auto p=reinterpret_cast<const unsigned char *>(accept); *p; ++p) table[*p]=true;
        std::size_t n=0; auto q=reinterpret_cast<const unsigned char *>(s);
        while (*q && table[*q]) {++n; ++q;}
        return n;
    }

    constexpr std::size_t strcspn(const char* s, const char* reject) noexcept {
        bool table[256]={false};
        for (auto p=reinterpret_cast<const unsigned char *>(reject); *p; ++p) table[*p]=true;
        std::size_t n=0; auto q=reinterpret_cast<const unsigned char *>(s);
        while (*q && !table[*q]) {++n; ++q;}
        return n;
    }

    constexpr char* strpbrk(char* s, const char* accept) noexcept {
        bool table[256]={false};
        for (auto p=reinterpret_cast<const unsigned char *>(accept); *p; ++p) table[*p]=true;
        for (auto q=reinterpret_cast<unsigned char *>(s); *q; ++q) if (table[*q]) return reinterpret_cast<char *>(q);
        return nullptr;
    }

    constexpr const char* strpbrk(const char* s, const char* accept) noexcept {
        return const_cast<const char*>(strpbrk(const_cast<char*>(s), accept));
    }

    constexpr char* strtok(char* s, const char* delim) noexcept {
        static char* save=nullptr;
        char* p=s?s:save;
        if (p == nullptr) return nullptr;
        auto is_delim=[delim](char c)->bool {
            for (const char* d=delim; *d; ++d) if (*d == c) return true;
            return false;
        };
        while (*p && is_delim(*p)) ++p;
        if (*p == '\0') {save=nullptr; return nullptr;}
        char* start=p;
        while (*p && !is_delim(*p)) ++p;
        if (*p) {*p='\0'; save=p+1;}
        else save=nullptr;
        return start;
    }

} // namespace mystl

#endif //CSTRING_H
