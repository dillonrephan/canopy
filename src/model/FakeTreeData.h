#pragma once

#include "FileNode.h"

class FakeTreeData
{
public:
    [[nodiscard]] static QVector<FileNode> create();
};
