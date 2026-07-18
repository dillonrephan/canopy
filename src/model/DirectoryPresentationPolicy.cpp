#include "DirectoryPresentationPolicy.h"

#include <algorithm>

DirectoryPresentation DirectoryPresentationPolicy::evaluate(
    const FileNode &directory)
{
    if (!directory.isDirectory())
        return {};

    const qsizetype visibleChildCount = std::min(
        directory.children.size(), VisualChildLimit);
    const quint64 visibleCount = static_cast<quint64>(visibleChildCount);
    const quint64 hiddenChildCount = directory.totalChildCount > visibleCount
        ? directory.totalChildCount - visibleCount
        : 0;
    const bool incomplete = directory.directoryState != DirectoryLoadState::Ready;

    return {
        visibleChildCount,
        hiddenChildCount,
        incomplete || hiddenChildCount > 0
    };
}
