
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <QtCore/private/qobject_p.h>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStringListModel>

#include <QtTest/QtTest>

#include "gdb/gdbmi.h"

#include <sys/types.h>
#include <unistd.h>


#undef NS
#ifdef QT_NAMESPACE
#   define STRINGIFY0(s) #s
#   define STRINGIFY1(s) STRINGIFY0(s)
#   define NS STRINGIFY1(QT_NAMESPACE) "::"
#   define NSX "'" STRINGIFY1(QT_NAMESPACE) "::"
#   define NSY "'"
#else
#   define NS ""
#   define NSX ""
#   define NSY ""
#endif

#define gettid()  QString("0x%1").arg((qulonglong)(void *)currentThread(), 0, 16)

using namespace Debugger;
using namespace Debugger::Internal;

typedef QList<QByteArray> QByteArrayList;

static QByteArray operator<<(QByteArray ba, const QByteArray &replacement)
{
    int pos = ba.indexOf('%');
    Q_ASSERT(pos != -1);
    return ba.replace(pos, 1, replacement);
}

static QByteArray &operator<<=(QByteArray &ba, const QByteArray &replacement)
{
    int pos = ba.indexOf('%');
    Q_ASSERT(pos != -1);
    return ba.replace(pos, 1, replacement);
}


template <typename T> 
inline QByteArray N(T t) { return QByteArray::number(t); }



struct Int3 {
    Int3() { i1 = 42; i2 = 43; i3 = 44; }
    int i1, i2, i3;
};

struct QString3 {
    QString3() { s1 = "a"; s2 = "b"; s3 = "c"; }
    QString s1, s2, s3;
};

class tst_Gdb;

class Thread : public QThread
{
    Q_OBJECT

public:
    Thread(tst_Gdb *test);
    void run();

public slots:
    void readStandardOutput();
    void readStandardError();
    void handleGdbStarted();
    void handleGdbError(QProcess::ProcessError);
    void handleGdbFinished(int, QProcess::ExitStatus);
    void writeToGdbRequested(const QByteArray &ba)
    {
        //qDebug() << "THREAD GDB IN: " << ba;
        m_proc->write(ba);
        m_proc->write("\n");
    }


public:
    QByteArray m_output;
    QByteArray m_error;
    QProcess *m_proc; // owned
    tst_Gdb *m_test; // not owned
};

class tst_Gdb : public QObject
{
    Q_OBJECT

public:
    tst_Gdb() : m_thread(this) {}

public slots:
    void dumperCompatibility();
    void dumpQAbstractItemAndModelIndex();
    void dumpQAbstractItemModel();
    void dumpQByteArray();
    void dumpQChar();
    void dumpQDateTime();
    void dumpQDir();
    void dumpQFile();
    void dumpQFileInfo();
    void dumpQHash();
    void dumpQHashNode();
    void dumpQImage();
    void dumpQImageData();
    void dumpQLinkedList();
    void dumpQList_int();
    void dumpQList_char();
    void dumpQList_QString();
    void dumpQList_QString3();
    void dumpQList_Int3();
    void dumpQLocale();
    void dumpQMap();
    void dumpQMapNode();
    void dumpQObject();
    void dumpQObjectChildList();
    void dumpQObjectMethodList();
    void dumpQObjectPropertyList();
    void dumpQObjectSignal();
    void dumpQObjectSignalList();
    void dumpQObjectSlot();
    void dumpQObjectSlotList();
    void dumpQPixmap();
    void dumpQSharedPointer();
    void dumpQTextCodec();
    void dumpQVariant_invalid();
    void dumpQVariant_QString();
    void dumpQVariant_QStringList();
    void dumpStdVector();
    void dumpQWeakPointer();
    void initTestCase();
    void cleanupTestCase();
    void runTestCase(const QByteArray &name,
        const QByteArray &type,
        const QByteArrayList &expexted);

signals:
    void writeToGdb(const QByteArray &ba);

private slots:
    void dumpQString();
    void dumpQStringList();

private:
    void dumpQAbstractItemHelper(QModelIndex &index);
    void dumpQAbstractItemModelHelper(QAbstractItemModel &m);
    void dumpQDateTimeHelper(const QDateTime &d);
    void dumpQFileHelper(const QString &name, bool exists);
    template <typename K, typename V> void dumpQHashNodeHelper(QHash<K, V> &hash);
    void dumpQImageHelper(const QImage &img);
    void dumpQImageDataHelper(QImage &img);
    template <typename T> void dumpQLinkedListHelper(QLinkedList<T> &l);
    void dumpQLocaleHelper(QLocale &loc);
    template <typename K, typename V> void dumpQMapHelper(QMap<K, V> &m);
    template <typename K, typename V> void dumpQMapNodeHelper(QMap<K, V> &m);
    void dumpQObjectChildListHelper(QObject &o);
    void dumpQObjectSignalHelper(QObject &o, int sigNum);
#if QT_VERSION >= 0x040500
    template <typename T>
    void dumpQSharedPointerHelper(QSharedPointer<T> &ptr);
    template <typename T>
    void dumpQWeakPointerHelper(QWeakPointer<T> &ptr);
#endif
    void dumpQTextCodecHelper(QTextCodec *codec);

private:
    Thread m_thread;
};

QMutex m_mutex;
QWaitCondition m_waitCondition;

//
// Dumpers
//

static void testDumper(QByteArray expected, const void *data, QByteArray outertype,
    bool dumpChildren, QByteArray actual____ = QByteArray(), 
    QByteArray = QByteArray(), int = 0, int = 0, int = 0, int = 0)
{
    Q_UNUSED(dumpChildren);
    expected = "locals={iname='local',name='Locals',value=' ',type=' ',"
        "children=[" + expected + "],arg=''}";
    char buf[100];
    sprintf(buf, "%p", data);
    //if ((!expected.startsWith('t') && !expected.startsWith('f'))
    //        || expected.startsWith("type"))
    //    expected = "tiname='$I',addr='$A'," + expected;
    expected.replace("$I", "iname");
    expected.replace("$T", QByteArray(outertype));
    expected.replace("$A", QByteArray(buf));
    if (actual____ != expected) {
        QByteArrayList l1 = actual____.split(',');
        QByteArrayList l2 = expected.split(',');
        for (int i = 0; i < l1.size() && i < l2.size(); ++i) {
            if (l1.at(i) == l2.at(i))
                qWarning() << "== " << l1.at(i);
            else 
                //qWarning() << "!= " << l1.at(i).right(30) << l2.at(i).right(30);
                qWarning() << "!= " << l1.at(i) << l2.at(i);
        }
        if (l1.size() != l2.size())
            qWarning() << "!= size: " << l1.size() << l2.size();
    }
    QCOMPARE(actual____, expected);
}

QByteArray str(const void *p)
{
    char buf[100];
    sprintf(buf, "%p", p);
    return buf;
}

static const void *deref(const void *p)
{
    return *reinterpret_cast<const char* const*>(p);
}

void tst_Gdb::dumperCompatibility()
{
    // Ensure that no arbitrary padding is introduced by QVectorTypedData.
    const size_t qVectorDataSize = 16;
    QCOMPARE(sizeof(QVectorData), qVectorDataSize);
    QVectorTypedData<int> *v = 0;
    QCOMPARE(size_t(&v->array), qVectorDataSize);
}

static const QByteArray utfToBase64(const QString &string)
{
    return QByteArray(reinterpret_cast<const char *>(string.utf16()), 2 * string.size()).toBase64();
}

static const char *boolToVal(bool b)
{
    return b ? "'true'" : "'false'";
}

static const QByteArray ptrToBa(const void *p, bool symbolicNull = true)
{
    return QByteArray().append(p == 0 && symbolicNull ?
        "<null>" :
        QByteArray("0x") + QByteArray::number((quintptr) p, 16));
}

static const QByteArray generateQStringSpec(const QString &str)
{
    return QByteArray("value='%',type='"NS"QString',numchild='0',valueencoded='2'")
        << utfToBase64(str);
}

static const QByteArray generateQCharSpec(const QChar& ch)
{
    return QByteArray("value='%',valueencoded='2',type='"NS"QChar',numchild='0'")
        << utfToBase64(QString(QLatin1String("'%1' (%2, 0x%3)")).
                   arg(ch).arg(ch.unicode()).arg(ch.unicode(), 0, 16));
}

static const QByteArray generateBoolSpec(bool b)
{
    return QByteArray("value=%,type='bool',numchild='0'")
        << boolToVal(b);
}

static const QByteArray generateLongSpec(long n)
{
    return QByteArray("value='%',type='long',numchild='0'")
        << N(qlonglong(n));
}

static const QByteArray generateIntSpec(int n)
{
    return QByteArray("value='%',type='int',numchild='0'")
        << N(n);
}

const QByteArray createExp(const void *ptr,
    const QByteArray &type, const QByteArray &method)
{
    return QByteArray("exp='(("NSX"%"NSY"*)%)->%'")
        << type << ptrToBa(ptr) << method;
}

// Helper functions.

#ifdef Q_CC_MSVC
#  define MAP_NODE_TYPE_END ">"
#else
#  define MAP_NODE_TYPE_END " >"
#endif

template <typename T> static const char *typeToString()
{
    return "<unknown type>";
}
template <typename T> const QByteArray valToString(const T &)
{
    return "<unknown value>";
}
template <> const QByteArray valToString(const int &n)
{
    return QByteArray().append(N(n));
}
template <> const QByteArray valToString(const QString &s)
{
    return QByteArray(utfToBase64(s)).append("',valueencoded='2");
}
template <> const QByteArray valToString(int * const &p)
{
    return ptrToBa(p);
}
template <typename T> const QByteArray derefValToString(const T &v)
{
    return valToString(v);
}
template <> const QByteArray derefValToString(int * const &ptr)
{
    return valToString(*ptr);
}
const QString stripPtrType(const QString &ptrTypeStr)
{
    return ptrTypeStr.mid(0, ptrTypeStr.size() - 2);
}
template <> const char *typeToString<int>()
{
    return "int";
}
template <> const char *typeToString<QString>()
{
    return NS"QString";
}
template <> const char *typeToString<int *>()
{
    return "int *";
}
template <typename T> bool isSimpleType()
{
    return false;
}
template <> bool isSimpleType<int>()
{
    return true;
}
template <typename T> bool isPointer()
{
    return false;
}
template <> bool isPointer<int *>()
{
    return true;
}

template <typename T> static const char *typeToNumchild()
{
    return "1";
}
template <> const char *typeToNumchild<int>()
{
    return "0";
}
template <> const char *typeToNumchild<QString>()
{
    return "0";
}
template <typename K, typename V>
QByteArray getMapType()
{
    return QByteArray(typeToString<K>()) + "@" + QByteArray(typeToString<V>());
}
template <typename K, typename V>
void getMapNodeParams(size_t &nodeSize, size_t &valOffset)
{
#if QT_VERSION >= 0x040500
    typedef QMapNode<K, V> NodeType;
    NodeType *node = 0;
    nodeSize = sizeof(NodeType);
    valOffset = size_t(&node->value);
#else
    nodeSize = sizeof(K) + sizeof(V) + 2*sizeof(void *);
    valOffset = sizeof(K);
#endif
}

void tst_Gdb::dumpQAbstractItemHelper(QModelIndex &index)
{
    const QAbstractItemModel *model = index.model();
    const QString &rowStr = N(index.row());
    const QString &colStr = N(index.column());
    const QByteArray &internalPtrStrSymbolic = ptrToBa(index.internalPointer());
    const QByteArray &internalPtrStrValue = ptrToBa(index.internalPointer(), false);
    const QByteArray &modelPtrStr = ptrToBa(model);
    QByteArray indexSpecSymbolic = QByteArray().append(rowStr + "," + colStr + ",").
        append(internalPtrStrSymbolic + "," + modelPtrStr);
    QByteArray indexSpecValue = QByteArray().append(rowStr + "," + colStr + ",").
        append(internalPtrStrValue + "," + modelPtrStr);
    QByteArray expected = QByteArray("tiname='iname',addr='").append(ptrToBa(&index)).
        append("',type='"NS"QAbstractItem',addr='$").append(indexSpecSymbolic).
        append("',value='").append(valToString(model->data(index).toString())).
        append("',numchild='1',children=[");
    int rowCount = model->rowCount(index);
    int columnCount = model->columnCount(index);
    for (int row = 0; row < rowCount; ++row) {
        for (int col = 0; col < columnCount; ++col) {
            const QModelIndex &childIndex = model->index(row, col, index);
            expected.append("{name='[").append(valToString(row)).append(",").
                append(N(col)).append("]',numchild='1',addr='$").
                append(N(childIndex.row())).append(",").
                append(N(childIndex.column())).append(",").
                append(ptrToBa(childIndex.internalPointer())).append(",").
                append(modelPtrStr).append("',type='"NS"QAbstractItem',value='").
                append(valToString(model->data(childIndex).toString())).append("'}");
            if (col < columnCount - 1 || row < rowCount - 1)
                expected.append(",");
        }
    }
    expected.append("]");
    testDumper(expected, &index, NS"QAbstractItem", true, indexSpecValue);
}

void tst_Gdb::dumpQAbstractItemAndModelIndex()
{
    class PseudoTreeItemModel : public QAbstractItemModel
    {
    public:
        PseudoTreeItemModel() : QAbstractItemModel(), parent1(0),
            parent1Child(1), parent2(10), parent2Child1(11), parent2Child2(12)
        {}

        int columnCount(const QModelIndex &parent = QModelIndex()) const
        {
            Q_UNUSED(parent);
            return 1;
        }

        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const
        {
            return !index.isValid() || role != Qt::DisplayRole ?
                    QVariant() : *static_cast<int *>(index.internalPointer());
        }

        QModelIndex index(int row, int column,
                          const QModelIndex & parent = QModelIndex()) const
        {
            QModelIndex index;
            if (column == 0) {
                if (!parent.isValid()) {
                    if (row == 0)
                        index = createIndex(row, column, &parent1);
                    else if (row == 1)
                        index = createIndex(row, column, &parent2);
                } else if (parent.internalPointer() == &parent1 && row == 0) {
                    index = createIndex(row, column, &parent1Child);
                } else if (parent.internalPointer() == &parent2) {
                    index = createIndex(row, column,
                                row == 0 ? &parent2Child1 : &parent2Child2);
                }
            }
            return index;
        }

        QModelIndex parent(const QModelIndex & index) const
        {
            QModelIndex parent;
            if (index.isValid()) {
                if (index.internalPointer() == &parent1Child)
                    parent = createIndex(0, 0, &parent1);
                else if (index.internalPointer() == &parent2Child1 ||
                         index.internalPointer() == &parent2Child2)
                    parent = createIndex(1, 0, &parent2);
            }
            return parent;
        }

        int rowCount(const QModelIndex &parent = QModelIndex()) const
        {
            int rowCount;
            if (!parent.isValid() || parent.internalPointer() == &parent2)
                rowCount = 2;
            else if (parent.internalPointer() == &parent1)
                rowCount = 1;
            else
                rowCount = 0;
            return rowCount;
        }

    private:
        mutable int parent1;
        mutable int parent1Child;
        mutable int parent2;
        mutable int parent2Child1;
        mutable int parent2Child2;
    };

    PseudoTreeItemModel m2;

    // Case 1: ModelIndex with no children.
    QStringListModel m(QStringList() << "item1" << "item2" << "item3");
    QModelIndex index = m.index(2, 0);

    testDumper(QByteArray("type='$T',value='(2, 0)',numchild='5',children=["
        "{name='row',value='2',type='int',numchild='0'},"
        "{name='column',value='0',type='int',numchild='0'},"
        "{name='parent',value='<invalid>',exp='(('$T'*)$A)->parent()',"
            "type='$T',numchild='1'},"
        "{name='internalId',%},"
        "{name='model',value='%',type='"NS"QAbstractItemModel*',"
            "numchild='1'}]")
         << generateQStringSpec(N(index.internalId()))
         << ptrToBa(&m),
        &index, NS"QModelIndex", true);

    // Case 2: ModelIndex with one child.
    QModelIndex index2 = m2.index(0, 0);
    dumpQAbstractItemHelper(index2);

    qDebug() << "FIXME: invalid indices should not have children";
    testDumper(QByteArray("type='$T',value='(0, 0)',numchild='5',children=["
        "{name='row',value='0',type='int',numchild='0'},"
        "{name='column',value='0',type='int',numchild='0'},"
        "{name='parent',value='<invalid>',exp='(('$T'*)$A)->parent()',"
            "type='$T',numchild='1'},"
        "{name='internalId',%},"
        "{name='model',value='%',type='"NS"QAbstractItemModel*',"
            "numchild='1'}]")
         << generateQStringSpec(N(index2.internalId()))
         << ptrToBa(&m2),
        &index2, NS"QModelIndex", true);


    // Case 3: ModelIndex with two children.
    QModelIndex index3 = m2.index(1, 0);
    dumpQAbstractItemHelper(index3);

    testDumper(QByteArray("type='$T',value='(1, 0)',numchild='5',children=["
        "{name='row',value='1',type='int',numchild='0'},"
        "{name='column',value='0',type='int',numchild='0'},"
        "{name='parent',value='<invalid>',exp='(('$T'*)$A)->parent()',"
            "type='$T',numchild='1'},"
        "{name='internalId',%},"
        "{name='model',value='%',type='"NS"QAbstractItemModel*',"
            "numchild='1'}]")
         << generateQStringSpec(N(index3.internalId()))
         << ptrToBa(&m2),
        &index3, NS"QModelIndex", true);


    // Case 4: ModelIndex with a parent.
    index = m2.index(0, 0, index3);
    testDumper(QByteArray("type='$T',value='(0, 0)',numchild='5',children=["
        "{name='row',value='0',type='int',numchild='0'},"
        "{name='column',value='0',type='int',numchild='0'},"
        "{name='parent',value='(1, 0)',exp='(('$T'*)$A)->parent()',"
            "type='$T',numchild='1'},"
        "{name='internalId',%},"
        "{name='model',value='%',type='"NS"QAbstractItemModel*',"
            "numchild='1'}]")
         << generateQStringSpec(N(index.internalId()))
         << ptrToBa(&m2),
        &index, NS"QModelIndex", true);


    // Case 5: Empty ModelIndex
    QModelIndex index4;
    testDumper("type='$T',value='<invalid>',numchild='0'",
        &index4, NS"QModelIndex", true);
}

void tst_Gdb::dumpQAbstractItemModelHelper(QAbstractItemModel &m)
{
    QByteArray address = ptrToBa(&m);
    QByteArray expected = QByteArray("tiname='iname',addr='%',"
        "type='"NS"QAbstractItemModel',value='(%,%)',numchild='1',children=["
        "{numchild='1',name='"NS"QObject',addr='%',value='%',"
        "valueencoded='2',type='"NS"QObject',displayedtype='%'}")
            << address
            << N(m.rowCount())
            << N(m.columnCount())
            << address
            << utfToBase64(m.objectName())
            << m.metaObject()->className();

    for (int row = 0; row < m.rowCount(); ++row) {
        for (int column = 0; column < m.columnCount(); ++column) {
            QModelIndex mi = m.index(row, column);
            expected.append(QByteArray(",{name='[%,%]',value='%',"
                "valueencoded='2',numchild='1',addr='$%,%,%,%',"
                "type='"NS"QAbstractItem'}")
                << N(row)
                << N(column)
                << utfToBase64(m.data(mi).toString())
                << N(mi.row())
                << N(mi.column())
                << ptrToBa(mi.internalPointer())
                << ptrToBa(mi.model()));
        }
    }
    expected.append("]");
    testDumper(expected, &m, NS"QAbstractItemModel", true);
}

void tst_Gdb::dumpQAbstractItemModel()
{
    // Case 1: No rows, one column.
    QStringList strList;
    QStringListModel model(strList);
    dumpQAbstractItemModelHelper(model);

    // Case 2: One row, one column.
    strList << "String 1";
    model.setStringList(strList);
    dumpQAbstractItemModelHelper(model);

    // Case 3: Two rows, one column.
    strList << "String 2";
    model.setStringList(strList);
    dumpQAbstractItemModelHelper(model);

    // Case 4: No rows, two columns.
    QStandardItemModel model2(0, 2);
    dumpQAbstractItemModelHelper(model2);

    // Case 5: One row, two columns.
    QStandardItem item1("Item (0,0)");
    QStandardItem item2("(Item (0,1)");
    model2.appendRow(QList<QStandardItem *>() << &item1 << &item2);
    dumpQAbstractItemModelHelper(model2);

    // Case 6: Two rows, two columns
    QStandardItem item3("Item (1,0");
    QStandardItem item4("Item (1,1)");
    model2.appendRow(QList<QStandardItem *>() << &item3 << &item4);
    dumpQAbstractItemModelHelper(model);
}

void tst_Gdb::dumpQByteArray()
{
    // Case 1: Empty object.
    QByteArray ba;
    testDumper("value='',valueencoded='1',type='"NS"QByteArray',numchild='0',"
            "childtype='char',childnumchild='0',children=[]",
        &ba, NS"QByteArray", true);

    // Case 2: One element.
    ba.append('a');
    testDumper("value='YQ==',valueencoded='1',type='"NS"QByteArray',numchild='1',"
            "childtype='char',childnumchild='0',children=[{value='61  (97 'a')'}]",
        &ba, NS"QByteArray", true);

    // Case 3: Two elements.
    ba.append('b');
    testDumper("value='YWI=',valueencoded='1',type='"NS"QByteArray',numchild='2',"
            "childtype='char',childnumchild='0',children=["
            "{value='61  (97 'a')'},{value='62  (98 'b')'}]",
        &ba, NS"QByteArray", true);

    // Case 4: > 100 elements.
    ba = QByteArray(101, 'a');
    QByteArray children;
    for (int i = 0; i < 101; i++)
        children.append("{value='61  (97 'a')'},");
    children.chop(1);
    testDumper(QByteArray("value='YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
            "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
            "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYQ== <size: 101, cut...>',"
            "valueencoded='1',type='"NS"QByteArray',numchild='101',"
            "childtype='char',childnumchild='0',children=[%]") << children,
        &ba, NS"QByteArray", true);

    // Case 5: Regular and special characters and the replacement character.
    ba = QByteArray("abc\a\n\r\e\'\"?");
    testDumper("value='YWJjBwoNGyciPw==',valueencoded='1',type='"NS"QByteArray',"
            "numchild='10',childtype='char',childnumchild='0',children=["
            "{value='61  (97 'a')'},{value='62  (98 'b')'},"
            "{value='63  (99 'c')'},{value='07  (7 '?')'},"
            "{value='0a  (10 '?')'},{value='0d  (13 '?')'},"
            "{value='1b  (27 '?')'},{value='27  (39 '?')'},"
            "{value='22  (34 '?')'},{value='3f  (63 '?')'}]",
        &ba, NS"QByteArray", true);
}

void tst_Gdb::dumpQChar()
{
    // Case 1: Printable ASCII character.
    QChar c('X');
    testDumper("value=''X', ucs=88',numchild='0'",
        &c, NS"QChar", false);

    // Case 2: Printable non-ASCII character.
    c = QChar(0x600);
    testDumper("value=''?', ucs=1536',numchild='0'",
        &c, NS"QChar", false);

    // Case 3: Non-printable ASCII character.
    c = QChar::fromAscii('\a');
    testDumper("value=''?', ucs=7',numchild='0'",
        &c, NS"QChar", false);

    // Case 4: Non-printable non-ASCII character.
    c = QChar(0x9f);
    testDumper("value=''?', ucs=159',numchild='0'",
        &c, NS"QChar", false);

    // Case 5: Printable ASCII Character that looks like the replacement character.
    c = QChar::fromAscii('?');
    testDumper("value=''?', ucs=63',numchild='0'",
        &c, NS"QChar", false);
}

void tst_Gdb::dumpQDateTimeHelper(const QDateTime &d)
{
    QByteArray value;
    if (d.isNull())
        value = "value='(null)'";
    else 
        value = QByteArray("value='%',valueencoded='2'")
            << utfToBase64(d.toString());

    QByteArray expected = QByteArray("%,type='$T',numchild='3',children=["
        "{name='isNull',%},"
        "{name='toTime_t',%},"
        "{name='toString',%},"
        "{name='toString_(ISO)',%},"
        "{name='toString_(SystemLocale)',%},"
        "{name='toString_(Locale)',%}]")
            << value
            << generateBoolSpec(d.isNull())
            << generateLongSpec((d.toTime_t()))
            << generateQStringSpec(d.toString())
            << generateQStringSpec(d.toString(Qt::ISODate))
            << generateQStringSpec(d.toString(Qt::SystemLocaleDate))
            << generateQStringSpec(d.toString(Qt::LocaleDate));
    testDumper(expected, &d, NS"QDateTime", true);
}

void tst_Gdb::dumpQDateTime()
{
    // Case 1: Null object.
    QDateTime d;
    dumpQDateTimeHelper(d);

    // Case 2: Non-null object.
    d = QDateTime::currentDateTime();
    dumpQDateTimeHelper(d);
}

void tst_Gdb::dumpQDir()
{
    // Case 1: Current working directory.
    QDir dir = QDir::current();
    testDumper(QByteArray("value='%',valueencoded='2',type='"NS"QDir',numchild='3',"
        "children=[{name='absolutePath',%},{name='canonicalPath',%}]")
            << utfToBase64(dir.absolutePath())
            << generateQStringSpec(dir.absolutePath())
            << generateQStringSpec(dir.canonicalPath()),
        &dir, NS"QDir", true);

    // Case 2: Root directory.
    dir = QDir::root();
    testDumper(QByteArray("value='%',valueencoded='2',type='"NS"QDir',numchild='3',"
        "children=[{name='absolutePath',%},{name='canonicalPath',%}]")
            << utfToBase64(dir.absolutePath())
            << generateQStringSpec(dir.absolutePath())
            << generateQStringSpec(dir.canonicalPath()),
        &dir, NS"QDir", true);
}

void tst_Gdb::dumpQFileHelper(const QString &name, bool exists)
{
    QFile file(name);
    QByteArray filenameAsBase64 = utfToBase64(name);
    testDumper(QByteArray("value='%',valueencoded='2',type='$T',numchild='2',"
        "children=[{name='fileName',value='%',type='"NS"QString',"
        "numchild='0',valueencoded='2'},"
        "{name='exists',value=%,type='bool',numchild='0'}]")
            << filenameAsBase64 << filenameAsBase64 << boolToVal(exists),
        &file, NS"QFile", true);
}

void tst_Gdb::dumpQFile()
{
    // Case 1: Empty file name => Does not exist.
    dumpQFileHelper("", false);

    // Case 2: File that is known to exist.
    QTemporaryFile file;
    file.open();
    dumpQFileHelper(file.fileName(), true);

    // Case 3: File with a name that most likely does not exist.
    dumpQFileHelper("jfjfdskjdflsdfjfdls", false);
}

void tst_Gdb::dumpQFileInfo()
{
    QFileInfo fi(".");
    QByteArray expected("value='%',valueencoded='2',type='$T',numchild='3',"
        "children=["
        "{name='absolutePath',%},"
        "{name='absoluteFilePath',%},"
        "{name='canonicalPath',%},"
        "{name='canonicalFilePath',%},"
        "{name='completeBaseName',%},"
        "{name='completeSuffix',%},"
        "{name='baseName',%},"
#ifdef QX
        "{name='isBundle',%},"
        "{name='bundleName',%},"
#endif
        "{name='fileName',%},"
        "{name='filePath',%},"
        "{name='group',%},"
        "{name='owner',%},"
        "{name='path',%},"
        "{name='groupid',%},"
        "{name='ownerid',%},"
        "{name='permissions',%},"
        "{name='caching',%},"
        "{name='exists',%},"
        "{name='isAbsolute',%},"
        "{name='isDir',%},"
        "{name='isExecutable',%},"
        "{name='isFile',%},"
        "{name='isHidden',%},"
        "{name='isReadable',%},"
        "{name='isRelative',%},"
        "{name='isRoot',%},"
        "{name='isSymLink',%},"
        "{name='isWritable',%},"
        "{name='created',value='%',valueencoded='2',%,"
            "type='"NS"QDateTime',numchild='1'},"
        "{name='lastModified',value='%',valueencoded='2',%,"
            "type='"NS"QDateTime',numchild='1'},"
        "{name='lastRead',value='%',valueencoded='2',%,"
            "type='"NS"QDateTime',numchild='1'}]");

    expected <<= utfToBase64(fi.filePath());
    expected <<= generateQStringSpec(fi.absolutePath());
    expected <<= generateQStringSpec(fi.absoluteFilePath());
    expected <<= generateQStringSpec(fi.canonicalPath());
    expected <<= generateQStringSpec(fi.canonicalFilePath());
    expected <<= generateQStringSpec(fi.completeBaseName());
    expected <<= generateQStringSpec(fi.completeSuffix());
    expected <<= generateQStringSpec(fi.baseName());
#ifdef Q_OS_MACX
    expected <<= generateBoolSpec(fi.isBundle());
    expected <<= generateQStringSpec(fi.bundleName());
#endif
    expected <<= generateQStringSpec(fi.fileName());
    expected <<= generateQStringSpec(fi.filePath());
    expected <<= generateQStringSpec(fi.group());
    expected <<= generateQStringSpec(fi.owner());
    expected <<= generateQStringSpec(fi.path());
    expected <<= generateLongSpec(fi.groupId());
    expected <<= generateLongSpec(fi.ownerId());
    expected <<= generateLongSpec(fi.permissions());
    expected <<= generateBoolSpec(fi.caching());
    expected <<= generateBoolSpec(fi.exists());
    expected <<= generateBoolSpec(fi.isAbsolute());
    expected <<= generateBoolSpec(fi.isDir());
    expected <<= generateBoolSpec(fi.isExecutable());
    expected <<= generateBoolSpec(fi.isFile());
    expected <<= generateBoolSpec(fi.isHidden());
    expected <<= generateBoolSpec(fi.isReadable());
    expected <<= generateBoolSpec(fi.isRelative());
    expected <<= generateBoolSpec(fi.isRoot());
    expected <<= generateBoolSpec(fi.isSymLink());
    expected <<= generateBoolSpec(fi.isWritable());
    expected <<= utfToBase64(fi.created().toString());
    expected <<= createExp(&fi, "QFileInfo", "created()");
    expected <<= utfToBase64(fi.lastModified().toString());
    expected <<= createExp(&fi, "QFileInfo", "lastModified()");
    expected <<= utfToBase64(fi.lastRead().toString());
    expected <<= createExp(&fi, "QFileInfo", "lastRead()");

    testDumper(expected, &fi, NS"QFileInfo", true);
}

void tst_Gdb::dumpQHash()
{
    QHash<QString, QList<int> > hash;
    hash.insert("Hallo", QList<int>());
    hash.insert("Welt", QList<int>() << 1);
    hash.insert("!", QList<int>() << 1 << 2);
    hash.insert("!", QList<int>() << 1 << 2);
}

template <typename K, typename V>
void tst_Gdb::dumpQHashNodeHelper(QHash<K, V> &hash)
{
    typename QHash<K, V>::iterator it = hash.begin();
    typedef QHashNode<K, V> HashNode;
    HashNode *dummy = 0;
    HashNode *node =
        reinterpret_cast<HashNode *>(reinterpret_cast<char *>(const_cast<K *>(&it.key())) -
            size_t(&dummy->key));
    const K &key = it.key();
    const V &val = it.value();
    QByteArray expected("value='");
    if (isSimpleType<V>())
        expected.append(valToString(val));
    expected.append("',numchild='2',children=[{name='key',type='").
        append(typeToString<K>()).append("',addr='").append(ptrToBa(&key)).
        append("'},{name='value',type='").append(typeToString<V>()).
        append("',addr='").append(ptrToBa(&val)).append("'}]");
    testDumper(expected, node, NS"QHashNode", true,
        getMapType<K, V>(), "", sizeof(it.key()), sizeof(it.value()));
}

void tst_Gdb::dumpQHashNode()
{
    // Case 1: simple type -> simple type.
    QHash<int, int> hash1;
    hash1[2] = 3;
    dumpQHashNodeHelper(hash1);

    // Case 2: simple type -> composite type.
    QHash<int, QString> hash2;
    hash2[5] = "String 7";
    dumpQHashNodeHelper(hash2);

    // Case 3: composite type -> simple type
    QHash<QString, int> hash3;
    hash3["String 11"] = 13;
    dumpQHashNodeHelper(hash3);

    // Case 4: composite type -> composite type
    QHash<QString, QString> hash4;
    hash4["String 17"] = "String 19";
    dumpQHashNodeHelper(hash4);
}

void tst_Gdb::dumpQImageHelper(const QImage &img)
{
    QByteArray expected = "value='(%x%)',type='"NS"QImage',numchild='1',"
        "children=[{name='data',type='"NS"QImageData',addr='%'}]"
            << N(img.width())
            << N(img.height())
            << ptrToBa(&img);
    testDumper(expected, &img, NS"QImage", true);
}

void tst_Gdb::dumpQImage()
{
    // Case 1: Null image.
    QImage img;
    dumpQImageHelper(img);

    // Case 2: Normal image.
    img = QImage(3, 700, QImage::Format_RGB555);
    dumpQImageHelper(img);

    // Case 3: Invalid image.
    img = QImage(100, 0, QImage::Format_Invalid);
    dumpQImageHelper(img);
}

void tst_Gdb::dumpQImageDataHelper(QImage &img)
{
    const QByteArray ba(QByteArray::fromRawData((const char*) img.bits(), img.numBytes()));
    QByteArray expected = QByteArray("tiname='$I',addr='$A',type='"NS"QImageData',").
        append("numchild='0',value='<hover here>',valuetooltipencoded='1',").
        append("valuetooltipsize='").append(N(ba.size())).append("',").
        append("valuetooltip='").append(ba.toBase64()).append("'");
    testDumper(expected, &img, NS"QImageData", false);
}

void tst_Gdb::dumpQImageData()
{
    // Case 1: Null image.
    QImage img;
    dumpQImageDataHelper(img);

    // Case 2: Normal image.
    img = QImage(3, 700, QImage::Format_RGB555);
    dumpQImageDataHelper(img);

    // Case 3: Invalid image.
    img = QImage(100, 0, QImage::Format_Invalid);
    dumpQImageDataHelper(img);
}

template <typename T>
        void tst_Gdb::dumpQLinkedListHelper(QLinkedList<T> &l)
{
    const int size = qMin(l.size(), 1000);
    const QString &sizeStr = N(size);
    const QByteArray elemTypeStr = typeToString<T>();
    QByteArray expected = QByteArray("value='<").append(sizeStr).
        append(" items>',valuedisabled='true',numchild='").append(sizeStr).
        append("',childtype='").append(elemTypeStr).append("',childnumchild='").
        append(typeToNumchild<T>()).append("',children=[");
    typename QLinkedList<T>::const_iterator iter = l.constBegin();
    for (int i = 0; i < size; ++i, ++iter) {
        expected.append("{");
        const T &curElem = *iter;
        if (isPointer<T>()) {
            const QString typeStr = stripPtrType(typeToString<T>());
            const QByteArray addrStr = valToString(curElem);
            if (curElem != 0) {
                expected.append("addr='").append(addrStr).append("',saddr='").
                        append(addrStr).append("',type='").append(typeStr).
                        append("',value='").
                        append(derefValToString(curElem)).append("'");
            } else {
                expected.append("addr='").append(ptrToBa(&curElem)).append("',type='").
                    append(typeStr).append("',value='<null>',numchild='0'");
            }
        } else {
            expected.append("addr='").append(ptrToBa(&curElem)).
                append("',value='").append(valToString(curElem)).append("'");
        }
        expected.append("}");
        if (i < size - 1)
            expected.append(",");
    }
    if (size < l.size())
        expected.append(",...");
    expected.append("]");
    testDumper(expected, &l, NS"QLinkedList", true, elemTypeStr);
}

void tst_Gdb::dumpQLinkedList()
{
    // Case 1: Simple element type.
    QLinkedList<int> l;

    // Case 1.1: Empty list.
    dumpQLinkedListHelper(l);

    // Case 1.2: One element.
    l.append(2);
    dumpQLinkedListHelper(l);

    // Case 1.3: Two elements
    l.append(3);
    dumpQLinkedListHelper(l);

    // Case 2: Composite element type.
    QLinkedList<QString> l2;

    // Case 2.1: Empty list.
    dumpQLinkedListHelper(l2);

    // Case 2.2: One element.
    l2.append("Teststring 1");
    dumpQLinkedListHelper(l2);

    // Case 2.3: Two elements.
    l2.append("Teststring 2");
    dumpQLinkedListHelper(l2);

    // Case 2.4: > 1000 elements.
    for (int i = 3; i <= 1002; ++i)
        l2.append("Test " + N(i));

    // Case 3: Pointer type.
    QLinkedList<int *> l3;
    l3.append(new int(5));
    l3.append(new int(7));
    l3.append(0);
    dumpQLinkedListHelper(l3);
}

    #if 0
    void tst_Gdb::dumpQLinkedList()
    {
        // Case 1: Simple element type.
        QLinkedList<int> l;

        // Case 1.1: Empty list.
        testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
            "childtype='int',childnumchild='0',children=[]",
            &l, NS"QLinkedList", true, "int");

        // Case 1.2: One element.
        l.append(2);
        testDumper("value='<1 items>',valuedisabled='true',numchild='1',"
            "childtype='int',childnumchild='0',children=[{addr='%',value='2'}]"
                << ptrToBa(l.constBegin().operator->()),
            &l, NS"QLinkedList", true, "int");

        // Case 1.3: Two elements
        l.append(3);
        QByteArray it0 = ptrToBa(l.constBegin().operator->());
        QByteArray it1 = ptrToBa(l.constBegin().operator++().operator->());
        testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
            "childtype='int',childnumchild='0',children=[{addr='%',value='2'},"
            "{addr='%',value='3'}]" << it0 << it1,
            &l, NS"QLinkedList", true, "int");

        // Case 2: Composite element type.
        QLinkedList<QString> l2;
        QLinkedList<QString>::const_iterator iter;

        // Case 2.1: Empty list.
        testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
            "childtype='"NS"QString',childnumchild='0',children=[]",
            &l2, NS"QLinkedList", true, NS"QString");

        // Case 2.2: One element.
        l2.append("Teststring 1");
        iter = l2.constBegin();
        qDebug() << *iter;
        testDumper("value='<1 items>',valuedisabled='true',numchild='1',"
            "childtype='"NS"QString',childnumchild='0',children=[{addr='%',value='%',}]"
                << ptrToBa(iter.operator->()) << utfToBase64(*iter),
            &l2, NS"QLinkedList", true, NS"QString");

        // Case 2.3: Two elements.
        QByteArray expected = "value='<2 items>',valuedisabled='true',numchild='2',"
            "childtype='int',childnumchild='0',children=[";
        iter = l2.constBegin();
        expected.append("{addr='%',%},"
            << ptrToBa(iter.operator->()) << utfToBase64(*iter));
        ++iter;
        expected.append("{addr='%',%}]"
            << ptrToBa(iter.operator->()) << utfToBase64(*iter));
        testDumper(expected,
            &l, NS"QLinkedList", true, NS"QString");

        // Case 2.4: > 1000 elements.
        for (int i = 3; i <= 1002; ++i)
            l2.append("Test " + N(i));

        expected = "value='<1002 items>',valuedisabled='true',"
            "numchild='1002',childtype='"NS"QString',childnumchild='0',children=['";
        iter = l2.constBegin();
        for (int i = 0; i < 1002; ++i, ++iter)
            expected.append("{addr='%',value='%'},"
                << ptrToBa(iter.operator->()) << utfToBase64(*iter));
        expected.append(",...]");
        testDumper(expected, &l, NS"QLinkedList", true, NS"QString");


        // Case 3: Pointer type.
        QLinkedList<int *> l3;
        l3.append(new int(5));
        l3.append(new int(7));
        l3.append(0);
        //dumpQLinkedListHelper(l3);
        testDumper("", &l, NS"QLinkedList", true, NS"QString");
    }
    #endif

void tst_Gdb::dumpQList_int()
{
    QList<int> ilist;
    testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
        "internal='1',children=[]",
        &ilist, NS"QList", true, "int");
    ilist.append(1);
    ilist.append(2);
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "internal='1',childtype='int',childnumchild='0',children=["
        "{addr='" + str(&ilist.at(0)) + "',value='1'},"
        "{addr='" + str(&ilist.at(1)) + "',value='2'}]",
        &ilist, NS"QList", true, "int");
}

void tst_Gdb::dumpQList_char()
{
    QList<char> clist;
    testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
        "internal='1',children=[]",
        &clist, NS"QList", true, "char");
    clist.append('a');
    clist.append('b');
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "internal='1',childtype='char',childnumchild='0',children=["
        "{addr='" + str(&clist.at(0)) + "',value=''a', ascii=97'},"
        "{addr='" + str(&clist.at(1)) + "',value=''b', ascii=98'}]",
        &clist, NS"QList", true, "char");
}

void tst_Gdb::dumpQList_QString()
{
    QList<QString> slist;
    testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
        "internal='1',children=[]",
        &slist, NS"QList", true, NS"QString");
    slist.append("a");
    slist.append("b");
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "internal='1',childtype='"NS"QString',childnumchild='0',children=["
        "{addr='" + str(&slist.at(0)) + "',value='YQA=',valueencoded='2'},"
        "{addr='" + str(&slist.at(1)) + "',value='YgA=',valueencoded='2'}]",
        &slist, NS"QList", true, NS"QString");
}

void tst_Gdb::dumpQList_Int3()
{
    QList<Int3> i3list;
    testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
        "internal='0',children=[]",
        &i3list, NS"QList", true, "Int3");
    i3list.append(Int3());
    i3list.append(Int3());
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "internal='0',childtype='Int3',children=["
        "{addr='" + str(&i3list.at(0)) + "'},"
        "{addr='" + str(&i3list.at(1)) + "'}]",
        &i3list, NS"QList", true, "Int3");
}

void tst_Gdb::dumpQList_QString3()
{
    QList<QString3> s3list;
    testDumper("value='<0 items>',valuedisabled='true',numchild='0',"
        "internal='0',children=[]",
        &s3list, NS"QList", true, "QString3");
    s3list.append(QString3());
    s3list.append(QString3());
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "internal='0',childtype='QString3',children=["
        "{addr='" + str(&s3list.at(0)) + "'},"
        "{addr='" + str(&s3list.at(1)) + "'}]",
        &s3list, NS"QList", true, "QString3");
}

void tst_Gdb::dumpQLocaleHelper(QLocale &loc)
{
    QByteArray expected = QByteArray("value='%',type='$T',numchild='8',"
            "children=[{name='country',%},"
            "{name='language',%},"
            "{name='measurementSystem',%},"
            "{name='numberOptions',%},"
            "{name='timeFormat_(short)',%},"
            "{name='timeFormat_(long)',%},"
            "{name='decimalPoint',%},"
            "{name='exponential',%},"
            "{name='percent',%},"
            "{name='zeroDigit',%},"
            "{name='groupSeparator',%},"
            "{name='negativeSign',%}]")
        << valToString(loc.name())
        << createExp(&loc, "QLocale", "country()")
        << createExp(&loc, "QLocale", "language()")
        << createExp(&loc, "QLocale", "measurementSystem()")
        << createExp(&loc, "QLocale", "numberOptions()")
        << generateQStringSpec(loc.timeFormat(QLocale::ShortFormat))
        << generateQStringSpec(loc.timeFormat())
        << generateQCharSpec(loc.decimalPoint())
        << generateQCharSpec(loc.exponential())
        << generateQCharSpec(loc.percent())
        << generateQCharSpec(loc.zeroDigit())
        << generateQCharSpec(loc.groupSeparator())
        << generateQCharSpec(loc.negativeSign());
    testDumper(expected, &loc, NS"QLocale", true);
}

void tst_Gdb::dumpQLocale()
{
    QLocale english(QLocale::English);
    dumpQLocaleHelper(english);

    QLocale german(QLocale::German);
    dumpQLocaleHelper(german);

    QLocale chinese(QLocale::Chinese);
    dumpQLocaleHelper(chinese);

    QLocale swahili(QLocale::Swahili);
    dumpQLocaleHelper(swahili);
}

template <typename K, typename V>
        void tst_Gdb::dumpQMapHelper(QMap<K, V> &map)
{
    QByteArray sizeStr(valToString(map.size()));
    size_t nodeSize;
    size_t valOff;
    getMapNodeParams<K, V>(nodeSize, valOff);
    int transKeyOffset = static_cast<int>(2*sizeof(void *)) - static_cast<int>(nodeSize);
    int transValOffset = transKeyOffset + valOff;
    bool simpleKey = isSimpleType<K>();
    bool simpleVal = isSimpleType<V>();
    QByteArray expected = QByteArray("value='<").append(sizeStr).append(" items>',numchild='").
        append(sizeStr).append("',extra='simplekey: ").append(N(simpleKey)).
        append(" isSimpleValue: ").append(N(simpleVal)).
        append(" keyOffset: ").append(N(transKeyOffset)).append(" valueOffset: ").
        append(N(transValOffset)).append(" mapnodesize: ").
        append(N(qulonglong(nodeSize))).append("',children=["); // 64bit Linux hack
    typedef typename QMap<K, V>::iterator mapIter;
    for (mapIter it = map.begin(); it != map.end(); ++it) {
        if (it != map.begin())
            expected.append(",");
        const QByteArray keyString =
            QByteArray(valToString(it.key())).replace("valueencoded","keyencoded");
        expected.append("{key='").append(keyString).append("',value='").
            append(valToString(it.value())).append("',");
        if (simpleKey && simpleVal) {
            expected.append("type='").append(typeToString<V>()).
                append("',addr='").append(ptrToBa(&it.value())).append("'");
        } else {
            QByteArray keyTypeStr = typeToString<K>();
            QByteArray valTypeStr = typeToString<V>();
#if QT_VERSION >= 0x040500
            expected.append("addr='").
                append(ptrToBa(reinterpret_cast<char *>(&(*it)) + sizeof(V))).
                append("',type='"NS"QMapNode<").append(keyTypeStr).append(",").
                append(valTypeStr).append(MAP_NODE_TYPE_END).append("'");
#else
            expected.append("type='"NS"QMapData::Node<").append(keyTypeStr).
                append(",").append(valTypeStr).append(MAP_NODE_TYPE_END).
                append("',exp='*('"NS"QMapData::Node<").append(keyTypeStr).
                append(",").append(valTypeStr).append(MAP_NODE_TYPE_END).
                append(" >'*)").append(ptrToBa(&(*it))).append("'");
#endif
        }
        expected.append("}");
    }
    expected.append("]");
    mapIter it = map.begin();
    testDumper(expected, *reinterpret_cast<QMapData **>(&it), NS"QMap",
               true, getMapType<K,V>(), "", 0, 0, nodeSize, valOff);
}

void tst_Gdb::dumpQMap()
{
    // Case 1: Simple type -> simple type.
    QMap<int, int> map1;

    // Case 1.1: Empty map.
    dumpQMapHelper(map1);

    // Case 1.2: One element.
    map1[2] = 3;
    dumpQMapHelper(map1);

    // Case 1.3: Two elements.
    map1[3] = 5;
    dumpQMapHelper(map1);

    // Case 2: Simple type -> composite type.
    QMap<int, QString> map2;

    // Case 2.1: Empty Map.
    dumpQMapHelper(map2);

    // Case 2.2: One element.
    map2[5] = "String 7";
    dumpQMapHelper(map2);

    // Case 2.3: Two elements.
    map2[7] = "String 11";
    dumpQMapHelper(map2);

    // Case 3: Composite type -> simple type.
    QMap<QString, int> map3;

    // Case 3.1: Empty map.
    dumpQMapHelper(map3);

    // Case 3.2: One element.
    map3["String 13"] = 11;
    dumpQMapHelper(map3);

    // Case 3.3: Two elements.
    map3["String 17"] = 13;
    dumpQMapHelper(map3);

    // Case 4: Composite type -> composite type.
    QMap<QString, QString> map4;

    // Case 4.1: Empty map.
    dumpQMapHelper(map4);

    // Case 4.2: One element.
    map4["String 19"] = "String 23";
    dumpQMapHelper(map4);

    // Case 4.3: Two elements.
    map4["String 29"] = "String 31";
    dumpQMapHelper(map4);

    // Case 4.4: Different value, same key (multimap functionality).
    map4["String 29"] = "String 37";
    dumpQMapHelper(map4);
}

template <typename K, typename V>
        void tst_Gdb::dumpQMapNodeHelper(QMap<K, V> &m)
{
    typename QMap<K, V>::iterator it = m.begin();
    const K &key = it.key();
    const V &val = it.value();
    //const char * const keyType = typeToString(key);
    QByteArray expected = QByteArray("value='',numchild='2',"
        "children=[{name='key',addr='").append(ptrToBa(&key)).
        append("',type='").append(typeToString<K>()).append("',value='").
        append(valToString(key)).append("'},{name='value',addr='").
        append(ptrToBa(&val)).append("',type='").append(typeToString<V>()).
        append("',value='").append(valToString(val)).
        append("'}]");
    size_t nodeSize;
    size_t valOffset;
    getMapNodeParams<K, V>(nodeSize, valOffset);
    testDumper(expected, *reinterpret_cast<QMapData **>(&it), NS"QMapNode",
               true, getMapType<K,V>(), "", 0, 0, nodeSize, valOffset);
}

void tst_Gdb::dumpQMapNode()
{
    // Case 1: simple type -> simple type.
    QMap<int, int> map;
    map[2] = 3;
    dumpQMapNodeHelper(map);

    // Case 2: simple type -> composite type.
    QMap<int, QString> map2;
    map2[3] = "String 5";
    dumpQMapNodeHelper(map2);

    // Case 3: composite type -> simple type.
    QMap<QString, int> map3;
    map3["String 7"] = 11;
    dumpQMapNodeHelper(map3);

    // Case 4: composite type -> composite type.
    QMap<QString, QString> map4;
    map4["String 13"] = "String 17";
    dumpQMapNodeHelper(map4);
}

void tst_Gdb::dumpQObject()
{
    QObject parent;
    testDumper("value='',valueencoded='2',type='$T',displayedtype='QObject',"
        "numchild='4'",
        &parent, NS"QObject", false);
    testDumper("value='',valueencoded='2',type='$T',displayedtype='QObject',"
        "numchild='4',children=["
        "{name='properties',addr='$A',type='$TPropertyList',"
            "value='<1 items>',numchild='1'},"
        "{name='signals',addr='$A',type='$TSignalList',"
            "value='<2 items>',numchild='2'},"
        "{name='slots',addr='$A',type='$TSlotList',"
            "value='<2 items>',numchild='2'},"
        "{name='parent',value='0x0',type='$T *',numchild='0'},"
        "{name='className',value='QObject',type='',numchild='0'}]",
        &parent, NS"QObject", true);

#if 0
    testDumper("numchild='2',value='<2 items>',type='QObjectSlotList',"
            "children=[{name='2',value='deleteLater()',"
            "numchild='0',addr='$A',type='QObjectSlot'},"
        "{name='3',value='_q_reregisterTimers(void*)',"
            "numchild='0',addr='$A',type='QObjectSlot'}]",
        &parent, NS"QObjectSlotList", true);
#endif

    parent.setObjectName("A Parent");
    testDumper("value='QQAgAFAAYQByAGUAbgB0AA==',valueencoded='2',type='$T',"
        "displayedtype='QObject',numchild='4'",
        &parent, NS"QObject", false);
    QObject child(&parent);
    testDumper("value='',valueencoded='2',type='$T',"
        "displayedtype='QObject',numchild='4'",
        &child, NS"QObject", false);
    child.setObjectName("A Child");
    QByteArray ba ="value='QQAgAEMAaABpAGwAZAA=',valueencoded='2',type='$T',"
        "displayedtype='QObject',numchild='4',children=["
        "{name='properties',addr='$A',type='$TPropertyList',"
            "value='<1 items>',numchild='1'},"
        "{name='signals',addr='$A',type='$TSignalList',"
            "value='<2 items>',numchild='2'},"
        "{name='slots',addr='$A',type='$TSlotList',"
            "value='<2 items>',numchild='2'},"
        "{name='parent',addr='" + str(&parent) + "',"
            "value='QQAgAFAAYQByAGUAbgB0AA==',valueencoded='2',type='$T',"
            "displayedtype='QObject',numchild='1'},"
        "{name='className',value='QObject',type='',numchild='0'}]";
    testDumper(ba, &child, NS"QObject", true);
    connect(&child, SIGNAL(destroyed()), qApp, SLOT(quit()));
    testDumper(ba, &child, NS"QObject", true);
    disconnect(&child, SIGNAL(destroyed()), qApp, SLOT(quit()));
    testDumper(ba, &child, NS"QObject", true);
    child.setObjectName("A renamed Child");
    testDumper("value='QQAgAHIAZQBuAGEAbQBlAGQAIABDAGgAaQBsAGQA',valueencoded='2',"
        "type='$T',displayedtype='QObject',numchild='4'",
        &child, NS"QObject", false);
}

void tst_Gdb::dumpQObjectChildListHelper(QObject &o)
{
    const QObjectList children = o.children();
    const int size = children.size();
    const QString sizeStr = N(size);
    QByteArray expected = QByteArray("numchild='").append(sizeStr).append("',value='<").
        append(sizeStr).append(" items>',type='"NS"QObjectChildList',children=[");
    for (int i = 0; i < size; ++i) {
        const QObject *child = children.at(i);
        expected.append("{addr='").append(ptrToBa(child)).append("',value='").
            append(utfToBase64(child->objectName())).
            append("',valueencoded='2',type='"NS"QObject',displayedtype='").
            append(child->metaObject()->className()).append("',numchild='1'}");
        if (i < size - 1)
            expected.append(",");
    }
    expected.append("]");
    testDumper(expected, &o, NS"QObjectChildList", true);
}

void tst_Gdb::dumpQObjectChildList()
{
    // Case 1: Object with no children.
    QObject o;
    dumpQObjectChildListHelper(o);

    // Case 2: Object with one child.
    QObject o2(&o);
    dumpQObjectChildListHelper(o);

    // Case 3: Object with two children.
    QObject o3(&o);
    dumpQObjectChildListHelper(o);
}

void tst_Gdb::dumpQObjectMethodList()
{
    QStringListModel m;
    testDumper("addr='<synthetic>',type='$T',numchild='20',"
        "childtype='"NS"QMetaMethod::Method',childnumchild='0',children=["
        "{name='0 0 destroyed(QObject*)',value='<Signal> (1)'},"
        "{name='1 1 destroyed()',value='<Signal> (1)'},"
        "{name='2 2 deleteLater()',value='<Slot> (2)'},"
        "{name='3 3 _q_reregisterTimers(void*)',value='<Slot> (2)'},"
        "{name='4 4 dataChanged(QModelIndex,QModelIndex)',value='<Signal> (1)'},"
        "{name='5 5 headerDataChanged(Qt::Orientation,int,int)',value='<Signal> (1)'},"
        "{name='6 6 layoutChanged()',value='<Signal> (1)'},"
        "{name='7 7 layoutAboutToBeChanged()',value='<Signal> (1)'},"
        "{name='8 8 rowsAboutToBeInserted(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='9 9 rowsInserted(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='10 10 rowsAboutToBeRemoved(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='11 11 rowsRemoved(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='12 12 columnsAboutToBeInserted(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='13 13 columnsInserted(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='14 14 columnsAboutToBeRemoved(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='15 15 columnsRemoved(QModelIndex,int,int)',value='<Signal> (1)'},"
        "{name='16 16 modelAboutToBeReset()',value='<Signal> (1)'},"
        "{name='17 17 modelReset()',value='<Signal> (1)'},"
        "{name='18 18 submit()',value='<Slot> (2)'},"
        "{name='19 19 revert()',value='<Slot> (2)'}]",
    &m, NS"QObjectMethodList", true);
}

void tst_Gdb::dumpQObjectPropertyList()
{
    // Case 1: Model without a parent.
    QStringListModel m(QStringList() << "Test1" << "Test2");
    testDumper("addr='<synthetic>',type='$T',numchild='1',value='<1 items>',"
        "children=[{name='objectName',type='QString',value='',"
            "valueencoded='2',numchild='0'}]",
         &m, NS"QObjectPropertyList", true);

    // Case 2: Model with a parent.
    QStringListModel m2(&m);
    testDumper("addr='<synthetic>',type='$T',numchild='1',value='<1 items>',"
        "children=[{name='objectName',type='QString',value='',"
            "valueencoded='2',numchild='0'}]",
         &m2, NS"QObjectPropertyList", true);
}

static const char *connectionType(uint type)
{
    Qt::ConnectionType connType = static_cast<Qt::ConnectionType>(type);
    const char *output = "unknown";
    switch (connType) {
        case Qt::AutoConnection: output = "auto"; break;
        case Qt::DirectConnection: output = "direct"; break;
        case Qt::QueuedConnection: output = "queued"; break;
        case Qt::BlockingQueuedConnection: output = "blockingqueued"; break;
        case 3: output = "autocompat"; break;
#if QT_VERSION >= 0x040600
        case Qt::UniqueConnection: break; // Can't happen.
#endif
        default:
            qWarning() << "Unknown connection type: " << type;
            break;
        };
    return output;
};

class Cheater : public QObject
{
public:
    static const QObjectPrivate *getPrivate(const QObject &o)
    {
        const Cheater &cheater = static_cast<const Cheater&>(o);
#if QT_VERSION >= 0x040600
        return dynamic_cast<const QObjectPrivate *>(cheater.d_ptr.data());
#else
        return dynamic_cast<const QObjectPrivate *>(cheater.d_ptr);
#endif
    }
};

typedef QVector<QObjectPrivate::ConnectionList> ConnLists;

void tst_Gdb::dumpQObjectSignalHelper(QObject &o, int sigNum)
{
    //qDebug() << o.objectName() << sigNum;
    QByteArray expected("addr='<synthetic>',numchild='1',type='"NS"QObjectSignal'");
#if QT_VERSION >= 0x040400
    expected.append(",children=[");
    const QObjectPrivate *p = Cheater::getPrivate(o);
    Q_ASSERT(p != 0);
    const ConnLists *connLists = reinterpret_cast<const ConnLists *>(p->connectionLists);
    QObjectPrivate::ConnectionList connList =
        connLists != 0 && connLists->size() > sigNum ?
        connLists->at(sigNum) : QObjectPrivate::ConnectionList();
    int i = 0;
    for (QObjectPrivate::Connection *conn = connList.first; conn != 0;
         ++i, conn = conn->nextConnectionList) {
        const QString iStr = N(i);
        expected.append("{name='").append(iStr).append(" receiver',");
        if (conn->receiver == &o)
            expected.append("value='<this>',type='").
                append(o.metaObject()->className()).
                append("',numchild='0',addr='").append(ptrToBa(&o)).append("'");
        else if (conn->receiver == 0)
            expected.append("value='0x0',type='"NS"QObject *',numchild='0'");
        else
            expected.append("addr='").append(ptrToBa(conn->receiver)).append("',value='").
                append(utfToBase64(conn->receiver->objectName())).append("',valueencoded='2',").
                append("type='"NS"QObject',displayedtype='").
                append(conn->receiver->metaObject()->className()).append("',numchild='1'");
        expected.append("},{name='").append(iStr).append(" slot',type='',value='");
        if (conn->receiver != 0)
            expected.append(conn->receiver->metaObject()->method(conn->method).signature());
        else
            expected.append("<invalid receiver>");
        expected.append("',numchild='0'},{name='").append(iStr).append(" type',type='',value='<").
            append(connectionType(conn->connectionType)).append(" connection>',").
            append("numchild='0'}");
        if (conn != connList.last)
            expected.append(",");
    }
    expected.append("],numchild='").append(N(i)).append("'");
#endif
    testDumper(expected, &o, NS"QObjectSignal", true, "", "", sigNum);
}

void tst_Gdb::dumpQObjectSignal()
{
    // Case 1: Simple QObject.
    QObject o;
    o.setObjectName("Test");
    testDumper("addr='<synthetic>',numchild='1',type='"NS"QObjectSignal',"
            "children=[],numchild='0'",
        &o, NS"QObjectSignal", true, "", "", 0);

    // Case 2: QAbstractItemModel with no connections.
    QStringListModel m(QStringList() << "Test1" << "Test2");
    for (int signalIndex = 0; signalIndex < 17; ++signalIndex)
        dumpQObjectSignalHelper(m, signalIndex);

    // Case 3: QAbstractItemModel with connections to itself and to another
    //         object, using different connection types.
    qRegisterMetaType<QModelIndex>("QModelIndex");
    connect(&m, SIGNAL(columnsAboutToBeInserted(const QModelIndex &, int, int)),
            &o, SLOT(deleteLater()), Qt::DirectConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(const QModelIndex &, int, int)),
            &m, SLOT(revert()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(const QModelIndex &, int, int)),
            &m, SLOT(submit()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsInserted(const QModelIndex &, int, int)),
            &m, SLOT(submit()), Qt::BlockingQueuedConnection);
    connect(&m, SIGNAL(columnsRemoved(const QModelIndex &, int, int)),
            &m, SLOT(deleteLater()), Qt::AutoConnection);
#if QT_VERSION >= 0x040600
    connect(&m, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)),
            &m, SLOT(revert()), Qt::UniqueConnection);
#endif
    for (int signalIndex = 0; signalIndex < 17; ++signalIndex)
        dumpQObjectSignalHelper(m, signalIndex);
}

void tst_Gdb::dumpQObjectSignalList()
{
    // Case 1: Simple QObject.
    QObject o;
    o.setObjectName("Test");

    testDumper("type='"NS"QObjectSignalList',value='<2 items>',"
                "addr='$A',numchild='2',children=["
            "{name='0',value='destroyed(QObject*)',numchild='0',"
                "addr='$A',type='"NS"QObjectSignal'},"
            "{name='1',value='destroyed()',numchild='0',"
                "addr='$A',type='"NS"QObjectSignal'}]",
        &o, NS"QObjectSignalList", true);

    // Case 2: QAbstractItemModel with no connections.
    QStringListModel m(QStringList() << "Test1" << "Test2");
    QByteArray expected = "type='"NS"QObjectSignalList',value='<16 items>',"
        "addr='$A',numchild='16',children=["
        "{name='0',value='destroyed(QObject*)',numchild='0',"
            "addr='$A',type='"NS"QObjectSignal'},"
        "{name='1',value='destroyed()',numchild='0',"
            "addr='$A',type='"NS"QObjectSignal'},"
        "{name='4',value='dataChanged(QModelIndex,QModelIndex)',numchild='0',"
            "addr='$A',type='"NS"QObjectSignal'},"
        "{name='5',value='headerDataChanged(Qt::Orientation,int,int)',"
            "numchild='0',addr='$A',type='"NS"QObjectSignal'},"
        "{name='6',value='layoutChanged()',numchild='0',"
            "addr='$A',type='"NS"QObjectSignal'},"
        "{name='7',value='layoutAboutToBeChanged()',numchild='0',"
            "addr='$A',type='"NS"QObjectSignal'},"
        "{name='8',value='rowsAboutToBeInserted(QModelIndex,int,int)',"
            "numchild='0',addr='$A',type='"NS"QObjectSignal'},"
        "{name='9',value='rowsInserted(QModelIndex,int,int)',"
            "numchild='0',addr='$A',type='"NS"QObjectSignal'},"
        "{name='10',value='rowsAboutToBeRemoved(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='11',value='rowsRemoved(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='12',value='columnsAboutToBeInserted(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='13',value='columnsInserted(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='14',value='columnsAboutToBeRemoved(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='15',value='columnsRemoved(QModelIndex,int,int)',"
            "numchild='%',addr='$A',type='"NS"QObjectSignal'},"
        "{name='16',value='modelAboutToBeReset()',"
            "numchild='0',addr='$A',type='"NS"QObjectSignal'},"
        "{name='17',value='modelReset()',"
            "numchild='0',addr='$A',type='"NS"QObjectSignal'}]";

    testDumper(expected << "0" << "0" << "0" << "0" << "0" << "0",
        &m, NS"QObjectSignalList", true);


    // Case 3: QAbstractItemModel with connections to itself and to another
    //         object, using different connection types.
    qRegisterMetaType<QModelIndex>("QModelIndex");
    connect(&m, SIGNAL(columnsAboutToBeInserted(QModelIndex, int, int)),
            &o, SLOT(deleteLater()), Qt::DirectConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(QModelIndex, int, int)),
            &m, SLOT(revert()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsInserted(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::BlockingQueuedConnection);
    connect(&m, SIGNAL(columnsRemoved(QModelIndex, int, int)),
            &m, SLOT(deleteLater()), Qt::AutoConnection);

    testDumper(expected << "1" << "1" << "2" << "1" << "0" << "0",
        &m, NS"QObjectSignalList", true);
}

QByteArray slotIndexList(const QObject *ob)
{
    QByteArray slotIndices;
    const QMetaObject *mo = ob->metaObject();
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod &mm = mo->method(i);
        if (mm.methodType() == QMetaMethod::Slot) {
            int slotIndex = mo->indexOfSlot(mm.signature());
            Q_ASSERT(slotIndex != -1);
            slotIndices.append(N(slotIndex));
            slotIndices.append(',');
        }
    }
    slotIndices.chop(1);
    return slotIndices;
}

void tst_Gdb::dumpQObjectSlot()
{
    // Case 1: Simple QObject.
    QObject o;
    o.setObjectName("Test");

    QByteArray slotIndices = slotIndexList(&o);
    QCOMPARE(slotIndices, QByteArray("2,3"));
    QCOMPARE(o.metaObject()->indexOfSlot("deleteLater()"), 2);

    QByteArray expected = QByteArray("addr='$A',numchild='1',type='$T',"
        //"children=[{name='1 sender'}],numchild='1'");
        "children=[],numchild='0'");
    qDebug() << "FIXME!";
    testDumper(expected, &o, NS"QObjectSlot", true, "", "", 2);


    // Case 2: QAbstractItemModel with no connections.
    QStringListModel m(QStringList() << "Test1" << "Test2");
    slotIndices = slotIndexList(&o);

    QCOMPARE(slotIndices, QByteArray("2,3"));
    QCOMPARE(o.metaObject()->indexOfSlot("deleteLater()"), 2);

    expected = QByteArray("addr='$A',numchild='1',type='$T',"
        //"children=[{name='1 sender'}],numchild='1'");
        "children=[],numchild='0'");
    qDebug() << "FIXME!";
    testDumper(expected, &o, NS"QObjectSlot", true, "", "", 2);


    // Case 3: QAbstractItemModel with connections to itself and to another
    //         o, using different connection types.
    qRegisterMetaType<QModelIndex>("QModelIndex");
    connect(&m, SIGNAL(columnsAboutToBeInserted(QModelIndex, int, int)),
            &o, SLOT(deleteLater()), Qt::DirectConnection);
    connect(&o, SIGNAL(destroyed(QObject *)),
            &m, SLOT(revert()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsInserted(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::BlockingQueuedConnection);
    connect(&m, SIGNAL(columnsRemoved(QModelIndex, int, int)),
            &m, SLOT(deleteLater()), Qt::AutoConnection);
#if QT_VERSION >= 0x040600
    connect(&m, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
            &m, SLOT(revert()), Qt::UniqueConnection);
#endif
    expected = QByteArray("addr='$A',numchild='1',type='$T',"
        //"children=[{name='1 sender'}],numchild='1'");
        "children=[],numchild='0'");
    qDebug() << "FIXME!";
    testDumper(expected, &o, NS"QObjectSlot", true, "", "", 2);

}

void tst_Gdb::dumpQObjectSlotList()
{
    // Case 1: Simple QObject.
    QObject o;
    o.setObjectName("Test");
    testDumper("numchild='2',value='<2 items>',type='$T',"
        "children=[{name='2',value='deleteLater()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='3',value='_q_reregisterTimers(void*)',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'}]",
        &o, NS"QObjectSlotList", true);

    // Case 2: QAbstractItemModel with no connections.
    QStringListModel m(QStringList() << "Test1" << "Test2");
    testDumper("numchild='4',value='<4 items>',type='$T',"
        "children=[{name='2',value='deleteLater()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='3',value='_q_reregisterTimers(void*)',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='18',value='submit()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='19',value='revert()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'}]",
        &m, NS"QObjectSlotList", true);

    // Case 3: QAbstractItemModel with connections to itself and to another
    //         object, using different connection types.
    qRegisterMetaType<QModelIndex>("QModelIndex");
    connect(&m, SIGNAL(columnsAboutToBeInserted(QModelIndex, int, int)),
            &o, SLOT(deleteLater()), Qt::DirectConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(QModelIndex, int, int)),
            &m, SLOT(revert()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsAboutToBeRemoved(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::QueuedConnection);
    connect(&m, SIGNAL(columnsInserted(QModelIndex, int, int)),
            &m, SLOT(submit()), Qt::BlockingQueuedConnection);
    connect(&m, SIGNAL(columnsRemoved(QModelIndex, int, int)),
            &m, SLOT(deleteLater()), Qt::AutoConnection);
    connect(&o, SIGNAL(destroyed(QObject *)), &m, SLOT(submit()));
    testDumper("numchild='4',value='<4 items>',type='$T',"
        "children=[{name='2',value='deleteLater()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='3',value='_q_reregisterTimers(void*)',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='18',value='submit()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'},"
        "{name='19',value='revert()',numchild='0',"
            "addr='$A',type='"NS"QObjectSlot'}]",
        &m, NS"QObjectSlotList", true);
}

void tst_Gdb::dumpQPixmap()
{
    // Case 1: Null Pixmap.
    QPixmap p;

    testDumper("value='(0x0)',type='$T',numchild='0'",
        &p, NS"QPixmap", true);


    // Case 2: Uninitialized non-null pixmap.
    p = QPixmap(20, 100);
    testDumper("value='(20x100)',type='$T',numchild='0'",
        &p, NS"QPixmap", true);


    // Case 3: Initialized non-null pixmap.
    const char * const pixmap[] = {
        "2 24 3 1", "       c None", ".      c #DBD3CB", "+      c #FCFBFA",
        "  ", "  ", "  ", ".+", ".+", ".+", ".+", ".+", ".+", ".+", ".+", ".+",
        ".+", ".+", ".+", ".+", ".+", ".+", ".+", ".+", ".+", "  ", "  ", "  "
    };
    p = QPixmap(pixmap);
    testDumper("value='(2x24)',type='$T',numchild='0'",
        &p, NS"QPixmap", true);
}

#if QT_VERSION >= 0x040500
template<typename T>
void tst_Gdb::dumpQSharedPointerHelper(QSharedPointer<T> &ptr)
{
    struct Cheater : public QSharedPointer<T>
    {
        static const typename QSharedPointer<T>::Data *getData(const QSharedPointer<T> &p)
        {
            return static_cast<const Cheater &>(p).d;
        }
    };

    QByteArray expected("value='");
    QString val1 = ptr.isNull() ? "<null>" : valToString(*ptr.data());
    QString val2 = isSimpleType<T>() ? val1 : "";
/*
    const int *weakAddr;
    const int *strongAddr;
    int weakValue;
    int strongValue;
    if (!ptr.isNull()) {
        weakAddr = reinterpret_cast<const int *>(&Cheater::getData(ptr)->weakref);
        strongAddr = reinterpret_cast<const int *>(&Cheater::getData(ptr)->strongref);
        weakValue = *weakAddr;
        strongValue = *strongAddr;
    } else {
        weakAddr = strongAddr = 0;
        weakValue = strongValue = 0;
    }
    expected.append(val2).append("',valuedisabled='true',numchild='1',children=[").
        append("{name='data',addr='").append(ptrToBa(ptr.data())).
        append("',type='").append(typeToString<T>()).append("',value='").append(val1).
        append("'},{name='weakref',value='").append(N(weakValue)).
        append("',type='int',addr='").append(ptrToBa(weakAddr)).append("',numchild='0'},").
        append("{name='strongref',value='").append(N(strongValue)).
        append("',type='int',addr='").append(ptrToBa(strongAddr)).append("',numchild='0'}]");
    testDumper(expected, &ptr, NS"QSharedPointer", true, typeToString<T>());
*/
}
#endif

void tst_Gdb::dumpQSharedPointer()
{
#if QT_VERSION >= 0x040500
    // Case 1: Simple type.
    // Case 1.1: Null pointer.
    QSharedPointer<int> simplePtr;
    dumpQSharedPointerHelper(simplePtr);

    // Case 1.2: Non-null pointer,
    QSharedPointer<int> simplePtr2(new int(99));
    dumpQSharedPointerHelper(simplePtr2);

    // Case 1.3: Shared pointer.
    QSharedPointer<int> simplePtr3 = simplePtr2;
    dumpQSharedPointerHelper(simplePtr2);

    // Case 1.4: Weak pointer.
    QWeakPointer<int> simplePtr4(simplePtr2);
    dumpQSharedPointerHelper(simplePtr2);

    // Case 2: Composite type.
    // Case 1.1: Null pointer.
    QSharedPointer<QString> compositePtr;
    // TODO: This case is not handled in gdbmacros.cpp (segfault!)
    //dumpQSharedPointerHelper(compoistePtr);

    // Case 1.2: Non-null pointer,
    QSharedPointer<QString> compositePtr2(new QString("Test"));
    dumpQSharedPointerHelper(compositePtr2);

    // Case 1.3: Shared pointer.
    QSharedPointer<QString> compositePtr3 = compositePtr2;
    dumpQSharedPointerHelper(compositePtr2);

    // Case 1.4: Weak pointer.
    QWeakPointer<QString> compositePtr4(compositePtr2);
    dumpQSharedPointerHelper(compositePtr2);
#endif
}

void tst_Gdb::dumpQVariant_invalid()
{
    QVariant v;
    testDumper("value='(invalid)',type='$T',numchild='0'",
        &v, NS"QVariant", false);
}

void tst_Gdb::dumpQVariant_QString()
{
    QVariant v = "abc";
    testDumper("value='KFFTdHJpbmcpICJhYmMi',valueencoded='5',type='$T',"
        "numchild='0'",
        &v, NS"QVariant", true);
/*
    FIXME: the QString version should have a child:
    testDumper("value='KFFTdHJpbmcpICJhYmMi',valueencoded='5',type='$T',"
        "numchild='1',children=[{name='value',value='IgBhAGIAYwAiAA==',"
        "valueencoded='4',type='QString',numchild='0'}]",
        &v, NS"QVariant", true);
*/
}

void tst_Gdb::dumpQVariant_QStringList()
{
    QVariant v = QStringList() << "Hi";
    testDumper("value='(QStringList) ',type='$T',numchild='1',"
        "children=[{name='value',exp='(*('myns::QStringList'*)%)',"
        "type='QStringList',numchild='1'}]"
            << QByteArray::number(quintptr(&v)),
        &v, NS"QVariant", true);
}

void tst_Gdb::dumpStdVector()
{
    std::vector<std::list<int> *> vector;
    QByteArray inner = "std::list<int> *";
    QByteArray innerp = "std::list<int>";
    testDumper("value='<0 items>',valuedisabled='true',numchild='0'",
        &vector, "std::vector", false, inner, "", sizeof(std::list<int> *));
    std::list<int> list;
    vector.push_back(new std::list<int>(list));
    testDumper("value='<1 items>',valuedisabled='true',numchild='1',"
        "childtype='" + inner + "',childnumchild='1',"
        "children=[{addr='" + str(deref(&vector[0])) + "',"
            "saddr='" + str(deref(&vector[0])) + "',type='" + innerp + "'}]",
        &vector, "std::vector", true, inner, "", sizeof(std::list<int> *));
    vector.push_back(0);
    list.push_back(45);
    testDumper("value='<2 items>',valuedisabled='true',numchild='2',"
        "childtype='" + inner + "',childnumchild='1',"
        "children=[{addr='" + str(deref(&vector[0])) + "',"
            "saddr='" + str(deref(&vector[0])) + "',type='" + innerp + "'},"
          "{addr='" + str(&vector[1]) + "',"
            "type='" + innerp + "',value='<null>',numchild='0'}]",
        &vector, "std::vector", true, inner, "", sizeof(std::list<int> *));
    vector.push_back(new std::list<int>(list));
    vector.push_back(0);
}

void tst_Gdb::dumpQTextCodecHelper(QTextCodec *codec)
{
    const QByteArray name = codec->name().toBase64();
    QByteArray expected = QByteArray("value='%',valueencoded='1',type='$T',"
        "numchild='2',children=[{name='name',value='%',type='"NS"QByteArray',"
        "numchild='0',valueencoded='1'},{name='mibEnum',%}]")
        << name << name << generateIntSpec(codec->mibEnum());
    testDumper(expected, codec, NS"QTextCodec", true);
}

void tst_Gdb::dumpQTextCodec()
{
    const QList<QByteArray> &codecNames = QTextCodec::availableCodecs();
    foreach (const QByteArray &codecName, codecNames)
        dumpQTextCodecHelper(QTextCodec::codecForName(codecName));
}

#if QT_VERSION >= 0x040500
template <typename T1, typename T2>
        size_t offsetOf(const T1 *klass, const T2 *member)
{
    return static_cast<size_t>(reinterpret_cast<const char *>(member) -
                               reinterpret_cast<const char *>(klass));
}

template <typename T>
void tst_Gdb::dumpQWeakPointerHelper(QWeakPointer<T> &ptr)
{
    typedef QtSharedPointer::ExternalRefCountData Data;
    const size_t dataOffset = 0;
    const Data *d = *reinterpret_cast<const Data **>(
            reinterpret_cast<const char **>(&ptr) + dataOffset);
    const int *weakRefPtr = reinterpret_cast<const int *>(&d->weakref);
    const int *strongRefPtr = reinterpret_cast<const int *>(&d->strongref);
    T *data = ptr.toStrongRef().data();
    const QString dataStr = valToString(*data);
    QByteArray expected("value='");
    if (isSimpleType<T>())
        expected.append(dataStr);
    expected.append("',valuedisabled='true',numchild='1',children=[{name='data',addr='").
        append(ptrToBa(data)).append("',type='").append(typeToString<T>()).
        append("',value='").append(dataStr).append("'},{name='weakref',value='").
        append(valToString(*weakRefPtr)).append("',type='int',addr='").
        append(ptrToBa(weakRefPtr)).append("',numchild='0'},{name='strongref',value='").
        append(valToString(*strongRefPtr)).append("',type='int',addr='").
        append(ptrToBa(strongRefPtr)).append("',numchild='0'}]");
    testDumper(expected, &ptr, NS"QWeakPointer", true, typeToString<T>());
}
#endif

void tst_Gdb::dumpQWeakPointer()
{
#if QT_VERSION >= 0x040500
    // Case 1: Simple type.

    // Case 1.1: Null pointer.
    QSharedPointer<int> spNull;
    QWeakPointer<int> wp = spNull.toWeakRef();
    testDumper("value='<null>',valuedisabled='true',numchild='0'",
        &wp, NS"QWeakPointer", true, "int");

    // Case 1.2: Weak pointer is unique.
    QSharedPointer<int> sp(new int(99));
    wp = sp.toWeakRef();
    dumpQWeakPointerHelper(wp);

    // Case 1.3: There are other weak pointers.
    QWeakPointer<int> wp2 = sp.toWeakRef();
    dumpQWeakPointerHelper(wp);

    // Case 1.4: There are other strong shared pointers as well.
    QSharedPointer<int> sp2(sp);
    dumpQWeakPointerHelper(wp);

    // Case 2: Composite type.
    QSharedPointer<QString> spS(new QString("Test"));
    QWeakPointer<QString> wpS = spS.toWeakRef();
    dumpQWeakPointerHelper(wpS);
#endif
}

#define VERIFY_OFFSETOF(member)                           \
do {                                                      \
    QObjectPrivate *qob = 0;                              \
    ObjectPrivate *ob = 0;                                \
    QVERIFY(size_t(&qob->member) == size_t(&ob->member)); \
} while (0)


Thread::Thread(tst_Gdb *test)
{
    moveToThread(this);
    m_test = test;
    m_proc = 0;
    m_proc = new QProcess;
    m_proc->moveToThread(this);
    qDebug() << "\nTHREAD CREATED" << getpid() << gettid();
    connect(m_test, SIGNAL(writeToGdb(QByteArray)),
        this, SLOT(writeToGdbRequested(QByteArray)));
    connect(m_proc, SIGNAL(error(QProcess::ProcessError)),
        this, SLOT(handleGdbError(QProcess::ProcessError)));
    connect(m_proc, SIGNAL(finished(int, QProcess::ExitStatus)),
        this, SLOT(handleGdbFinished(int, QProcess::ExitStatus)));
    connect(m_proc, SIGNAL(started()),
        this, SLOT(handleGdbStarted()));
    connect(m_proc, SIGNAL(readyReadStandardOutput()),
        this, SLOT(readStandardOutput()));
    connect(m_proc, SIGNAL(readyReadStandardError()),
        this, SLOT(readStandardError()));
    start();
}

void Thread::handleGdbError(QProcess::ProcessError)
{
    qDebug() << "GDB ERROR: ";
}

void Thread::handleGdbFinished(int, QProcess::ExitStatus)
{
    qDebug() << "GDB FINISHED: ";
}

void Thread::readStandardOutput()
{
    QByteArray ba = m_proc->readAllStandardOutput();
    if (ba.isEmpty())
        return;
    // =library-loaded...
    if (ba.startsWith("=")) 
        return;
    //if (ba.startsWith("~"))
    //    return;
    if (!ba.startsWith("~\"locals="))
        return;
    //qDebug() << "THREAD GDB OUT: " << ba;
    //m_output += ba;
    ba = ba.mid(2, ba.size() - 4);
    ba = ba.replace("\\\"", "\"");
    m_output = ba;
    m_waitCondition.wakeAll();
}

void Thread::readStandardError()
{
    return;
    QByteArray ba = m_proc->readAllStandardOutput();
    qDebug() << "THREAD GDB ERR: " << ba;
    m_error += ba;
    m_waitCondition.wakeAll();
}

void Thread::handleGdbStarted()
{
    //qDebug() << "\n\nGDB STARTED" << getpid() << gettid() << "\n\n";
}

void Thread::run()
{
    //qDebug() << "\nTHREAD RUN" << getpid() << gettid();
    m_proc->start("./gdb -i mi --args ./tst_gdb run");
    m_proc->waitForStarted();
    m_proc->write("b main\n");
    m_proc->write("run\n");
    m_proc->write("handle SIGSTOP stop pass\n");
    //qDebug() << "\nTHREAD RUNNING";
    exec();
}

void tst_Gdb::initTestCase()
{
    // FIXME: Wait until gdb proc is running.
    QTest::qWait(1000);
}


void tst_Gdb::runTestCase(const QByteArray &name, const QByteArray &type,
    const QByteArrayList &expected)
{
    //qDebug() << "\nABOUT TO RUN TEST: " << name << m_thread.m_proc;

    writeToGdb("b " + name);

    for (int i = 0; i != expected.size(); ++i) {
        if (i == 0)
            writeToGdb("call " + name + "()");
        else
            writeToGdb("next");
        writeToGdb("bb");
        m_mutex.lock();
        m_waitCondition.wait(&m_mutex);
        QByteArray ba = m_thread.m_output;
        m_mutex.unlock();
        //GdbMi locals;
        //locals.fromString("{" + ba + "}");
        QByteArray received = ba.replace("\"", "'");
        //qDebug() << "OUTPUT: " << ba << "\n\n";
        //qDebug() << "OUTPUT: " << locals.toString() << "\n\n";
        testDumper(expected.at(i), 0, type, false, received);
    }
}

void tst_Gdb::cleanupTestCase()
{
    writeToGdb("kill");
    writeToGdb("quit");
    //m_thread.m_proc->waitForFinished();
}

void dumpQStringTest()
{
    QString s;
    s = "hallo";
    s += "x";
    s += "y";
}

void tst_Gdb::dumpQString()
{
    QByteArrayList bal;

    bal.append("{iname='local.s',addr='0xbffff19c',name='S',"
        "type='"NS"QString',value='<not in scope>',numchild='0'}");

    //bal.append("xxx");
    //bal.append("xxx");
    runTestCase("dumpQStringTest", NS"QString", bal);
/*
    testDumper("value='',valueencoded='2',type='$T',numchild='0'",
        &s, NS"QString", false);
    s = "abc";
    testDumper("value='YQBiAGMA',valueencoded='2',type='$T',numchild='0'",
        &s, NS"QString", false);
*/
}

void dumpQStringListTest()
{
    QStringList s;
}

void tst_Gdb::dumpQStringList()
{
    QByteArrayList bal;
    //bal.append("xxx");
    runTestCase("dumpQStringListTest", NS"QStringList", bal);
}

int runit(int &argc, char *argv[])
{
    // Plain call. Start the testing.
    QApplication app(argc, argv);
    tst_Gdb test;
    return QTest::qExec(&test, argc, argv);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && QByteArray(argv[1]) == "run") {
        // We are the debugged process, recursively called and steered
        // by our spawning alter ego.
        return 0;
    }

    return runit(argc, argv);
}

#include "tst_gdb.moc"

