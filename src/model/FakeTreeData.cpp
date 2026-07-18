#include "FakeTreeData.h"

#include "DirectoryPresentationPolicy.h"

namespace {
constexpr quint64 kExtremeDirectoryCount = 1'000'000;

NodeId addNode(QVector<FileNode> &nodes,
               NodeId parentId,
               QString name,
               NodeKind kind,
               QString detail)
{
    const NodeId id = static_cast<NodeId>(nodes.size() + 1);
    QString path;
    if (parentId == InvalidNodeId) {
        path = QStringLiteral("/home/dillon");
    } else {
        path = nodes.at(static_cast<qsizetype>(parentId - 1)).path + u'/' + name;
        nodes[static_cast<qsizetype>(parentId - 1)].children.append(id);
    }

    nodes.append({id, parentId, std::move(name), std::move(path), kind,
                  std::move(detail), {}});
    return id;
}

void finalizeDirectoryCounts(QVector<FileNode> &nodes)
{
    for (FileNode &node : nodes) {
        if (node.isDirectory() && node.totalChildCount == 0)
            node.totalChildCount = static_cast<quint64>(node.children.size());
    }
}
}

QVector<FileNode> FakeTreeData::create()
{
    QVector<FileNode> nodes;
    nodes.reserve(72);

    const NodeId home = addNode(nodes, InvalidNodeId, QStringLiteral("Home"),
                                NodeKind::Directory, QStringLiteral("5 main branches"));
    const NodeId documents = addNode(nodes, home, QStringLiteral("Documents"),
                                     NodeKind::Directory, QStringLiteral("Notes and records"));
    const NodeId pictures = addNode(nodes, home, QStringLiteral("Pictures"),
                                    NodeKind::Directory, QStringLiteral("2 image flowers"));
    const NodeId music = addNode(nodes, home, QStringLiteral("Music"),
                                 NodeKind::Directory, QStringLiteral("Audio collection"));
    const NodeId downloads = addNode(nodes, home, QStringLiteral("Downloads"),
                                     NodeKind::Directory, QStringLiteral("Crowded fixture · 14 items"));
    const NodeId projects = addNode(nodes, home, QStringLiteral("Projects"),
                                    NodeKind::Directory, QStringLiteral("Active work"));

    addNode(nodes, documents, QStringLiteral("field-notes.md"), NodeKind::Document,
            QStringLiteral("Markdown · 18 KB"));
    const NodeId receipts = addNode(nodes, documents, QStringLiteral("Receipts"),
                                    NodeKind::Directory, QStringLiteral("12 documents"));
    addNode(nodes, receipts, QStringLiteral("june.pdf"), NodeKind::Document,
            QStringLiteral("PDF document · 420 KB"));
    addNode(nodes, receipts, QStringLiteral("july.pdf"), NodeKind::Document,
            QStringLiteral("PDF document · 386 KB"));

    addNode(nodes, pictures, QStringLiteral("cedar-trail.jpg"), NodeKind::Image,
            QStringLiteral("JPEG image · 4.8 MB"));
    addNode(nodes, pictures, QStringLiteral("sketch.png"), NodeKind::Image,
            QStringLiteral("PNG image · 1.2 MB"));
    addNode(nodes, music, QStringLiteral("quiet-hours.flac"), NodeKind::Audio,
            QStringLiteral("FLAC audio · 31 MB"));

    const QStringList downloadNames {
        QStringLiteral("canopy-notes.txt"), QStringLiteral("wallpaper.png"),
        QStringLiteral("sample-pack.zip"), QStringLiteral("forest-loop.flac"),
        QStringLiteral("invoice.pdf"), QStringLiteral("seed-data.csv"),
        QStringLiteral("reference.jpg"), QStringLiteral("release.tar.zst"),
        QStringLiteral("ideas.md"), QStringLiteral("palette.ase"),
        QStringLiteral("branch-study.svg"), QStringLiteral("demo.mov"),
        QStringLiteral("unicode-芽.txt"), QStringLiteral("very-long-filename-for-layout-testing.txt")
    };
    for (qsizetype i = 0; i < downloadNames.size(); ++i) {
        const NodeKind kind = i == 1 || i == 6 ? NodeKind::Image
            : i == 2 || i == 7 ? NodeKind::Archive
            : i == 3 ? NodeKind::Audio
            : NodeKind::Document;
        addNode(nodes, downloads, downloadNames.at(i), kind,
                QStringLiteral("Fake download · %1 KB").arg(24 + i * 37));
    }

    const NodeId canopy = addNode(nodes, projects, QStringLiteral("Canopy"),
                                  NodeKind::Directory, QStringLiteral("Qt 6 project"));
    addNode(nodes, projects, QStringLiteral("archive.tar.zst"), NodeKind::Archive,
            QStringLiteral("Compressed archive · 82 MB"));
    const NodeId source = addNode(nodes, canopy, QStringLiteral("src"),
                                  NodeKind::Directory, QStringLiteral("C++ sources"));
    addNode(nodes, canopy, QStringLiteral("README.md"), NodeKind::Document,
            QStringLiteral("Markdown · 4 KB"));
    addNode(nodes, source, QStringLiteral("main.cpp"), NodeKind::Code,
            QStringLiteral("C++ source · 1 KB"));
    addNode(nodes, source, QStringLiteral("TreeSceneItem.cpp"), NodeKind::Code,
            QStringLiteral("C++ source · 12 KB"));

    const NodeId largeDataset = addNode(
        nodes, projects, QStringLiteral("Large Dataset"), NodeKind::Directory,
        QStringLiteral("1,000,000 entries · bounded sample"));
    for (qsizetype i = 0; i < DirectoryPresentationPolicy::VisualChildLimit; ++i) {
        addNode(nodes,
                largeDataset,
                QStringLiteral("sample-%1.dat").arg(i + 1, 6, 10, QLatin1Char('0')),
                NodeKind::Document,
                QStringLiteral("Sample entry · metadata deferred"));
    }

    FileNode &largeDirectory = nodes[static_cast<qsizetype>(largeDataset - 1)];
    largeDirectory.totalChildCount = kExtremeDirectoryCount;
    largeDirectory.directoryState = DirectoryLoadState::Partial;
    largeDirectory.directoryStatus = QStringLiteral(
        "Showing a bounded sample while the remaining entries stay unloaded.");

    finalizeDirectoryCounts(nodes);
    return nodes;
}
