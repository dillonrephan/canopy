#pragma once

#include "FileNode.h"

struct DirectoryPresentation {
    qsizetype visibleChildCount = 0;
    quint64 hiddenChildCount = 0;
    bool usesContentsFallback = false;
};

class DirectoryPresentationPolicy
{
public:
    static constexpr qsizetype VisualChildLimit = 24;

    [[nodiscard]] static DirectoryPresentation evaluate(const FileNode &directory);
};
