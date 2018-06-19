#include <QApplication>

#include "guiutil.h"

#include "bitcoinaddressvalidator.h"
#include "walletmodel.h"
#include "bitcoinunits.h"

#include "util.h"
#include "init.h"

#include <QDateTime>
#include <QDoubleValidator>
#include <QFont>
#include <QLineEdit>
#include <QUrl>
#include <QTextDocument> // For Qt::escape
#include <QAbstractItemView>
#include <QClipboard>
#include <QFileDialog>
#include <QDesktopServices>
#include <QThread>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "shlwapi.h"
#include "shlobj.h"
#include "shellapi.h"
#endif

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date)
{
    return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QFont bitcoinAddressFont()
{
    QFont font("Monospace");
#if QT_VERSION >= 0x040800
    font.setStyleHint(QFont::Monospace);
#else
    font.setStyleHint(QFont::TypeWriter);
#endif
    return font;
}

void setupAddressWidget(QLineEdit *widget, QWidget *parent)
{
    widget->setMaxLength(BitcoinAddressValidator::MaxAddressLength);
    widget->setValidator(new BitcoinAddressValidator(parent));
    widget->setFont(bitcoinAddressFont());
}

void setupAmountWidget(QLineEdit *widget, QWidget *parent)
{
    QDoubleValidator *amountValidator = new QDoubleValidator(parent);
    amountValidator->setDecimals(8);
    amountValidator->setBottom(0.0);
    widget->setValidator(amountValidator);
    widget->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // boostcoin: check prefix
    if(uri.scheme() != QString("boostcoin"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    rv.amount = 0;
    QList<QPair<QString, QString> > items = uri.queryItems();
    for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fShouldReturnFalse = false;
        if (i->first.startsWith("req-"))
        {
            i->first.remove(0, 4);
            fShouldReturnFalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fShouldReturnFalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isEmpty())
            {
                if(!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fShouldReturnFalse = false;
        }

        if (fShouldReturnFalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    // Convert boostcoin:// to boostcoin:
    //
    //    Cannot handle this later, because bitcoin:// will cause Qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).
    if(uri.startsWith("boostcoin://"))
    {
        uri.replace(0, 12, "boostcoin:");
    }
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
    QString escaped = Qt::escape(str);
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        QApplication::clipboard()->setText(selection.at(0).data(role).toString());
    }
}

QString getSaveFileName(QWidget *parent, const QString &caption,
                                 const QString &dir,
                                 const QString &filter,
                                 QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    QString result = QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter);

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = qApp->widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void openDebugLogfile()
{
    boost::filesystem::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (boost::filesystem::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(pathDebug.string())));
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold, QObject *parent) :
    QObject(parent), size_threshold(size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt>") && !Qt::mightBeRichText(tooltip))
        {
            // Prefix <qt/> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "<qt/>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

#ifdef WIN32
boost::filesystem::path static StartupShortcutPath()
{
    return GetSpecialFolderPath(CSIDL_STARTUP) / "boostcoin.lnk";
}

bool GetStartOnSystemStartup()
{
    // check for Bitcoin.lnk
    return boost::filesystem::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    boost::filesystem::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(NULL);

        // Get a pointer to the IShellLink interface.
        IShellLink* psl = NULL;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL,
                                CLSCTX_INPROC_SERVER, IID_IShellLink,
                                reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            TCHAR pszExePath[MAX_PATH];
            GetModuleFileName(NULL, pszExePath, sizeof(pszExePath));

            TCHAR pszArgs[5] = TEXT("-min");

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpec(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(pszArgs);

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = NULL;
            hres = psl->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                WCHAR pwsz[MAX_PATH];
                // Ensure that the string is ANSI.
                MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().string().c_str(), -1, pwsz, MAX_PATH);
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(pwsz, TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}

#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
//  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

boost::filesystem::path static GetAutostartDir()
{
    namespace fs = boost::filesystem;

    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

boost::filesystem::path static GetAutostartFilePath()
{
    return GetAutostartDir() / "boostcoin.desktop";
}

bool GetStartOnSystemStartup()
{
    boost::filesystem::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        boost::filesystem::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        memset(pszExePath, 0, sizeof(pszExePath));
        if (readlink("/proc/self/exe", pszExePath, sizeof(pszExePath)-1) == -1)
            return false;

        boost::filesystem::create_directories(GetAutostartDir());

        boost::filesystem::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out|std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        // Write a bitcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        optionFile << "Name=boostcoin\n";
        optionFile << "Exec=" << pszExePath << " -min\n";
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}
#else

// TODO: OSX startup stuff; see:
// https://developer.apple.com/library/mac/#documentation/MacOSX/Conceptual/BPSystemStartup/Articles/CustomLogin.html

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

HelpMessageBox::HelpMessageBox(QWidget *parent) :
    QMessageBox(parent)
{
    header = tr("boostcoin-Qt") + " " + tr("version") + " " +
        QString::fromStdString(FormatFullVersion()) + "\n\n" +
        tr("Usage:") + "\n" +
        "  boostcoin-qt [" + tr("command-line options") + "]                     " + "\n";

    coreOptions = QString::fromStdString(HelpMessage());

    uiOptions = tr("UI options") + ":\n" +
        "  -lang=<lang>           " + tr("Set language, for example \"de_DE\" (default: system locale)") + "\n" +
        "  -min                   " + tr("Start minimized") + "\n" +
        "  -splash                " + tr("Show splash screen on startup (default: 1)") + "\n";

    setWindowTitle(tr("boostcoin-Qt"));
    setTextFormat(Qt::PlainText);
    // setMinimumWidth is ignored for QMessageBox so put in non-breaking spaces to make it wider.
    setText(header + QString(QChar(0x2003)).repeated(50));
    setDetailedText(coreOptions + "\n" + uiOptions);
}

void HelpMessageBox::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions;
    fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void HelpMessageBox::showOrPrint()
{
#if defined(WIN32)
        // On Windows, show a message box, as there is no stderr/stdout in windowed applications
        exec();
#else
        // On other operating systems, print help text to console
        printToConsole();
#endif
}

void SetThemeQSS(QApplication& app)
{

#ifdef Q_OS_MAC
// Mac OS styles here


#elif _WIN32
// Windows styles here
    app.setStyleSheet(
        "QMainWindow            { background-color: rgb(44,45,128) ;}"
        "QDialog                { background-color: rgb(44,45,128) ;}"
        "QFrame                 { border: none ;}"
        "QFrame#SendCoinsEntry  { background-color: rgba(0,0,0,0); color: rgb(44,45,128) ;}"
        "QMenu                  { font-family: Arial,Gadget,sans-serif ;}"

        "QWidget#widget           { padding: 2px; margin: 3px; background-color: rgb(208,224,239); color: rgb(44,45,128); border-radius: 8px; padding: 4px ;}"
        "QWidget#widget_2         { padding: 2px; margin: 3px; border: 2px solid rgb(43,196,237); color: rgb(255,255,255); border-radius: 8px; padding: 4px ;}"

        "QPushButton            { font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(51,47,202); color: rgb(255,255,252); border: 2px solid rgb(51,47,202); border-radius: 4px; padding: 3px;  margin: 6px; margin-left: 12px; margin-right: 12px ;}"
        "QPushButton:hover      { font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(51,47,202); color: rgb(255,255,252); border: 2px solid rgb(43,196,237); border-radius: 4px; padding: 3px;  margin: 6px; margin-left: 12px; margin-right: 12px ;}"
        "QPushButton:disabled   { color: rgb(150,150,150) ;}"

        "QToolBar               { background-color: rgb(51,47,202) ;}"

        "QTabBar::tab           { color: rgb(51,47,202); border: 1px solid rgb(255,255,255); border-bottom: none; padding: 5px ;}"
        "QTabBar::tab:selected  { background: rgb(43,196,237) ;}"
        "QTabBar::tab:!selected { background: rgb(255,255,255); margin-top: 2px ;}"

        "QToolButton            { font-size: 14px; font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(51,47,202); color: rgb(255,255,252); border: 3px solid rgb(51,47,202); border-radius: 10px; padding: 8px;  margin: 8px; margin-left: 12px; margin-right: 12px ;}"
        "QToolButton:hover      { font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(51,47,202); color: rgb(255,255,252); border: 3px solid rgb(43,196,237); border-radius: 10px; padding: 8px;  margin: 8px; margin-left: 12px; margin-right: 12px ;}"
        "QToolButton:pressed    { font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(43,196,237); color: rgb(255,255,252); border: 3px solid rgb(43,196,237); border-radius: 10px; padding: 8px;  margin: 8px; margin-left: 12px; margin-right: 12px ;}"
        "QToolButton:checked    { font-weight: bold; font-family: Arial,Gadget,sans-serif; background: rgb(51,47,202); color: rgb(255,255,252); border: 3px solid rgb(43,196,237); border-radius: 10px; padding: 8px;  margin: 8px; margin-left: 12px; margin-right: 12px ;}"
        "QToolButton:disabled   { color: rgb(150,150,150) ;}"

        "QDoubleSpinBox         { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QLineEdit              { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QTextEdit              { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"
        "QTextEdit#messagesWidget      { padding: 2px; border-radius: 3px; background: rgb(44,45,128); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QPlainTextEdit         { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QMenu                  { background: rgb(255,255,255); color: rgb(51,47,202) ;}"
        "QMenu::item:selected   { background: rgb(43,196,237); color: rgb(255,255,255) ;}"

        "QMenuBar               { background: rgb(255,255,255); color: rgb(51,47,202) ;}"
        "QMenuBar::item         { background: rgb(255,255,255); color: rgb(51,47,202) ;}"
        "QMenuBar::item:selected { background: rgb(43,196,237) ;}"

        "QLabel                 { font-family: Arial,Gadget,sans-serif; color: rgb(255,255,255) ;}"
        "QLabel::header         { border-color: rgb(51,47,202) ;}"
        "QLabel::progressBarLabel { color: rgb(255,255,255) ;}"

        "QScrollBar             { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QCheckBox              { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QRadioButton           { padding: 2px; border-radius: 3px; background: rgb(51,47,202); color: rgb(208,224,239); border-color: rgb(255,255,253) ;}"

        "QTabWidget::tab-bar    { border-radius: 2px ;}"
        "QTabWidget::pane       { border: 2px solid rgb(43,196,237); border-radius: 4px ;}"

        "QProgressBar           { font-weight: bold; color: rgb(51,47,202); border-color: rgb(255,255,255); border-radius: 4px ;}"
        "QProgressBar::chunk    { background: rgb(43,196,237) ;}"

        "QTreeView              { background-color: rgba(0,0,0,0) ;}"
        "QTreeView::item        { background: rgb(51,47,202); color: rgb(255,255,255) ;}"
        "QTreeView::item:selected { background-color: rgba(0,0,0,0) ;}"

        "QTableView             { font-family: Arial,Gadget,sans-serif; background-color: transparent; color: rgb(255,255,255); selection-background-color: rgb(90,47,202) ;}"

        "QScrollArea#scrollArea { background-color: rgba(0,0,0,0) ;}"

        "QHeaderView::section   { font-family: Arial,Gadget,sans-serif;  padding: 1px; background: rgb(255,255,255); color: rgb(51,47,202) ;}"
        "QHeaderView            { font-family: Arial,Gadget,sans-serif;  padding: 1px; background: rgb(255,255,255) ;}"

        "QWidget#transactionsPage { background-color: rgba(0,0,0,0) ;}"
        "QWidget#scrollAreaWidgetContents { background-color: rgba(0,0,0,0) ;}"

        "QDialog#EditAddressDialog, QDialog#SignVerifyMessageDialog, QDialog#AskPassphraseDialog, QDialog#CoinControlDialog, QDialog#TransactionDescDialog, QDialog#AboutDialog, QDialog#OptionsDialog, QDialog#QRCodeDialog, QDialog#RPCConsole, QMessageBox  {color: rgb(44,45,128) ;}"

        "QSpinBox               { font-family: Arial,Gadget,sans-serif;  border-radius: 2px; color: rgb(255,255,255); background: rgb(51,47,202) ;}"
        "QComboBox              { font-family: Arial,Gadget,sans-serif;  border-radius: 2px; color: rgb(255,255,255); background: rgb(51,47,202) ;}"
        "QComboBox QAbstractItemView  {font-family: Arial,Gadget,sans-serif;   border-radius: 2px; color: rgb(255,255,255); background: rgb(51,47,202) ;}"

        "QVBoxLayout            { background: rgb(255,255,255) ;}"

        "QLabel#overviewpage_spendable_label, QLabel#overviewpage_stake_label, QLabel#overviewpage_unconfirmed_label, QLabel#overviewpage_immature_label, QLabel#labelBalance, QLabel#labelStake, QLabel#labelUnconfirmed, QLabel#labelImmature {font-weight: bold; font-family: Arial,Gadget,sans-serif; font-size: 12px; color: rgb(51,47,202);}"
        "QLabel#label_MyWallet {color: rgb(51,47,202) ;}"
        "QLabel#overviewpage_total_label, QLabel#labelTotal {font-weight: bold; font-family: Arial,Gadget,sans-serif; font-size: 16px; color: rgb(51,47,202) ;}"
        "QLabel#lblAmount, QLabel#lblLabel, QLabel#lblMessage {font-weight: bold; font-family: Arial,Gadget,sans-serif; color: rgb(51,47,202) ;}"

        "QListView              { font-family: Arial,Gadget,sans-serif; border-color: rgb(208,224,239) color: rgb(255,255,255); show-decoration-selected: 1 ;}"

    );
}

#else
// Linux styles here


#endif

} // namespace GUIUtil

