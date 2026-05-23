#pragma once

#include <QString>

class Solution;

class SolutionSerializer
{
public:
    // Writes solution to <rootPath>/<name>.sspec. Returns true on success.
    static bool save(Solution* solution, QString& errorOut);

    // Reads a .sspec file, creates and returns a Solution (caller owns it).
    // Returns nullptr on failure and populates errorOut.
    static Solution* load(const QString& sspecPath, QString& errorOut);

    static constexpr const char* kExtension = ".sspec";
};
