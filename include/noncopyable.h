#ifndef NONCOPYABLE_H_INCLUDED
#define NONCOPYABLE_H_INCLUDED

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable()=default;
    ~noncopyable()=default;
};

#endif // NONCOPYABLE_H_INCLUDED
