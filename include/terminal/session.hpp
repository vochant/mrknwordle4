#pragma once

class TermSession {
    bool handlesInterrupts;

public:
    TermSession();
    ~TermSession();
    TermSession(const TermSession&) = delete;
    TermSession& operator=(const TermSession&) = delete;
};
