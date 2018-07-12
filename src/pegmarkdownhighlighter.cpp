#include "pegmarkdownhighlighter.h"

#include <QTextDocument>
#include <QTimer>
#include <QDebug>

#include "pegparser.h"
#include "vconfigmanager.h"
#include "utils/vutils.h"

extern VConfigManager *g_config;

PegMarkdownHighlighter::PegMarkdownHighlighter(QTextDocument *p_doc)
    : QSyntaxHighlighter(p_doc),
      m_doc(p_doc),
      m_timeStamp(0),
      m_parser(NULL),
      m_parserExts(pmh_EXT_STRIKE | pmh_EXT_FRONTMATTER)
{
}

void PegMarkdownHighlighter::init(const QVector<HighlightingStyle> &p_styles,
                                  const QHash<QString, QTextCharFormat> &p_codeBlockStyles,
                                  bool p_mathjaxEnabled,
                                  int p_timerInterval)
{
    m_styles = p_styles;
    m_codeBlockStyles = p_codeBlockStyles;

    m_codeBlockFormat.setForeground(QBrush(Qt::darkYellow));
    for (int index = 0; index < m_styles.size(); ++index) {
        switch (m_styles[index].type) {
        case pmh_FENCEDCODEBLOCK:
            m_codeBlockFormat = m_styles[index].format;
            break;

        default:
            break;
        }
    }

    m_colorColumnFormat = m_codeBlockFormat;
    m_colorColumnFormat.setForeground(QColor(g_config->getEditorColorColumnFg()));
    m_colorColumnFormat.setBackground(QColor(g_config->getEditorColorColumnBg()));

    m_result.reset(new PegHighlighterResult());

    m_parser = new PegParser(this);
    connect(m_parser, &PegParser::parseResultReady,
            this, &PegMarkdownHighlighter::handleParseResult);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(p_timerInterval);
    connect(m_timer, &QTimer::timeout,
            this, [this]() {
                startParse();
            });

    connect(m_doc, &QTextDocument::contentsChange,
            this, &PegMarkdownHighlighter::handleContentsChange);
}

void PegMarkdownHighlighter::highlightBlock(const QString &p_text)
{
    QSharedPointer<PegHighlighterResult> result(m_result);

    int blockNum = currentBlock().blockNumber();
    if (result->m_blocksHighlights.size() > blockNum) {
        // units are sorted by start position and length.
        const QVector<HLUnit> &units = result->m_blocksHighlights[blockNum];
        if (!units.isEmpty()) {
            for (int i = 0; i < units.size(); ++i) {
                const HLUnit &unit = units[i];
                if (i == 0) {
                    // No need to merge format.
                    setFormat(unit.start,
                              unit.length,
                              m_styles[unit.styleIndex].format);
                } else {
                    QTextCharFormat newFormat = m_styles[unit.styleIndex].format;
                    for (int j = i - 1; j >= 0; --j) {
                        if (units[j].start + units[j].length <= unit.start) {
                            // It won't affect current unit.
                            continue;
                        } else {
                            // Merge the format.
                            QTextCharFormat tmpFormat(newFormat);
                            newFormat = m_styles[units[j].styleIndex].format;
                            // tmpFormat takes precedence.
                            newFormat.merge(tmpFormat);
                        }
                    }

                    setFormat(unit.start, unit.length, newFormat);
                }
            }
        }
    }

    // Set current block's user data.
    updateBlockUserData(blockNum, p_text);

    setCurrentBlockState(HighlightBlockState::Normal);

    updateCodeBlockState(result, blockNum, p_text);

    if (currentBlockState() == HighlightBlockState::CodeBlock) {
        highlightCodeBlock(result, blockNum);

        highlightCodeBlockColorColumn(p_text);
    }
}

void PegMarkdownHighlighter::handleContentsChange(int p_position, int p_charsRemoved, int p_charsAdded)
{
    Q_UNUSED(p_position);

    if (p_charsRemoved == 0 && p_charsAdded == 0) {
        return;
    }

    ++m_timeStamp;

    // We still need a timer to start a complete parse.
    m_timer->start();
}

void PegMarkdownHighlighter::startParse()
{
    QSharedPointer<PegParseConfig> config(new PegParseConfig());
    config->m_timeStamp = m_timeStamp;
    config->m_data = m_doc->toPlainText().toUtf8();
    config->m_numOfBlocks = m_doc->blockCount();
    config->m_extensions = m_parserExts;

    m_parser->parseAsync(config);
}

static bool compHLUnitStyle(const HLUnitStyle &a, const HLUnitStyle &b)
{
    if (a.start < b.start) {
        return true;
    } else if (a.start == b.start) {
        return a.length > b.length;
    } else {
        return false;
    }
}

void PegMarkdownHighlighter::setCodeBlockHighlights(TimeStamp p_timeStamp,
                                                    const QVector<HLUnitPos> &p_units)
{
    QSharedPointer<PegHighlighterResult> result(m_result);

    if (!result->matched(p_timeStamp)) {
        return;
    }

    if (p_units.isEmpty()) {
        goto exit;
    }

    {
    QVector<QVector<HLUnitStyle>> highlights(result->m_codeBlocksHighlights.size());
    for (auto const &unit : p_units) {
        int pos = unit.m_position;
        int end = unit.m_position + unit.m_length;
        QTextBlock block = m_doc->findBlock(pos);
        int startBlockNum = block.blockNumber();
        int endBlockNum = m_doc->findBlock(end).blockNumber();

        // Text has been changed. Abandon the obsolete parsed result.
        if (startBlockNum == -1 || endBlockNum >= highlights.size()) {
            goto exit;
        }

        while (block.isValid()) {
            int blockNumber = block.blockNumber();
            if (blockNumber > endBlockNum) {
                break;
            }

            int blockStartPos = block.position();
            HLUnitStyle hl;
            hl.style = unit.m_style;
            if (blockNumber == startBlockNum) {
                hl.start = pos - blockStartPos;
                hl.length = (startBlockNum == endBlockNum) ?
                                (end - pos) : (block.length() - hl.start);
            } else if (blockNumber == endBlockNum) {
                hl.start = 0;
                hl.length = end - blockStartPos;
            } else {
                hl.start = 0;
                hl.length = block.length();
            }

            highlights[blockNumber].append(hl);

            block = block.next();
        }
    }

    // Need to highlight in order.
    for (int i = 0; i < highlights.size(); ++i) {
        QVector<HLUnitStyle> &units = highlights[i];
        if (!units.isEmpty()) {
            if (units.size() > 1) {
                std::sort(units.begin(), units.end(), compHLUnitStyle);
            }

            result->m_codeBlocksHighlights[i].append(units);
        }
    }
    }

exit:
    if (--result->m_numOfCodeBlockHighlightsToRecv <= 0) {
        rehighlight();
    }
}

void PegMarkdownHighlighter::updateHighlightFast()
{
    updateHighlight();
}

void PegMarkdownHighlighter::updateHighlight()
{
    m_timer->stop();
    startParse();
}

void PegMarkdownHighlighter::handleParseResult(const QSharedPointer<PegParseResult> &p_result)
{
    if (!m_result.isNull() && m_result->m_timeStamp > p_result->m_timeStamp) {
        return;
    }

    PegHighlighterResult *pegRes = new PegHighlighterResult(this, p_result);
    m_result.reset(pegRes);

    updateCodeBlocks(m_result);

    // Now we got a new result, rehighlight.
    rehighlight();

    completeHighlight(m_result);
}

void PegMarkdownHighlighter::updateCodeBlocks(QSharedPointer<PegHighlighterResult> p_result)
{
    if (!p_result->matched(m_timeStamp)) {
        return;
    }

    if (g_config->getEnableCodeBlockHighlight()) {
        p_result->m_codeBlocksHighlights.resize(p_result->m_numOfBlocks);
        p_result->m_numOfCodeBlockHighlightsToRecv = p_result->m_codeBlocks.size();
    }

    emit codeBlocksUpdated(p_result->m_timeStamp, p_result->m_codeBlocks);
}

void PegMarkdownHighlighter::updateBlockUserData(int p_blockNum, const QString &p_text)
{
    Q_UNUSED(p_text);
    VTextBlockData *blockData = currentBlockData();
    if (!blockData) {
        blockData = new VTextBlockData();
        setCurrentBlockUserData(blockData);
    } else {
        blockData->setCodeBlockIndentation(-1);
        blockData->clearMathjax();
    }

    if (blockData->getPreviews().isEmpty()) {
        m_possiblePreviewBlocks.remove(p_blockNum);
    } else {
        m_possiblePreviewBlocks.insert(p_blockNum);
    }
}

void PegMarkdownHighlighter::updateCodeBlockState(const QSharedPointer<PegHighlighterResult> &p_result,
                                                  int p_blockNum,
                                                  const QString &p_text)
{
    if (!p_result->matched(m_timeStamp)) {
        return;
    }

    auto it = p_result->m_codeBlocksState.find(p_blockNum);
    if (it != p_result->m_codeBlocksState.end()) {
        VTextBlockData *blockData = currentBlockData();
        Q_ASSERT(blockData);

        HighlightBlockState state = it.value();
        // Set code block indentation.
        switch (state) {
        case HighlightBlockState::CodeBlockStart:
        {
            int startLeadingSpaces = 0;
            QRegExp reg(VUtils::c_fencedCodeBlockStartRegExp);
            int idx = reg.indexIn(p_text);
            if (idx >= 0) {
                startLeadingSpaces = reg.capturedTexts()[1].size();
            }

            blockData->setCodeBlockIndentation(startLeadingSpaces);
            break;
        }

        case HighlightBlockState::CodeBlock:
            V_FALLTHROUGH;
        case HighlightBlockState::CodeBlockEnd:
        {
            int startLeadingSpaces = 0;
            VTextBlockData *preBlockData = previousBlockData();
            if (preBlockData) {
                startLeadingSpaces = preBlockData->getCodeBlockIndentation();
            }

            blockData->setCodeBlockIndentation(startLeadingSpaces);
            break;
        }

        default:
            Q_ASSERT(false);
            break;
        }

        // Set code block state.
        setCurrentBlockState(state);
    }
}

void PegMarkdownHighlighter::highlightCodeBlock(const QSharedPointer<PegHighlighterResult> &p_result,
                                                int p_blockNum)
{
    if (p_result->m_codeBlocksHighlights.size() > p_blockNum) {
        const QVector<HLUnitStyle> &units = p_result->m_codeBlocksHighlights[p_blockNum];
        if (!units.isEmpty()) {
            QVector<QTextCharFormat *> formats(units.size(), NULL);
            for (int i = 0; i < units.size(); ++i) {
                const HLUnitStyle &unit = units[i];
                auto it = m_codeBlockStyles.find(unit.style);
                if (it == m_codeBlockStyles.end()) {
                    continue;
                }

                formats[i] = &(*it);

                QTextCharFormat newFormat = m_codeBlockFormat;
                newFormat.merge(*it);
                for (int j = i - 1; j >= 0; --j) {
                    if (units[j].start + units[j].length <= unit.start) {
                        // It won't affect current unit.
                        continue;
                    } else {
                        // Merge the format.
                        if (formats[j]) {
                            QTextCharFormat tmpFormat(newFormat);
                            newFormat = *(formats[j]);
                            // tmpFormat takes precedence.
                            newFormat.merge(tmpFormat);
                        }
                    }
                }

                setFormat(unit.start, unit.length, newFormat);
            }
        }
    }
}

void PegMarkdownHighlighter::highlightCodeBlockColorColumn(const QString &p_text)
{
    int cc = g_config->getColorColumn();
    if (cc <= 0) {
        return;
    }

    VTextBlockData *blockData = currentBlockData();
    Q_ASSERT(blockData);
    int indent = blockData->getCodeBlockIndentation();
    if (indent == -1) {
        return;
    }

    cc += indent;
    if (p_text.size() < cc) {
        return;
    }

    setFormat(cc - 1, 1, m_colorColumnFormat);
}

void PegMarkdownHighlighter::completeHighlight(QSharedPointer<PegHighlighterResult> p_result)
{
    if (!p_result->matched(m_timeStamp)) {
        return;
    }

    emit imageLinksUpdated(p_result->m_imageRegions);
    emit headersUpdated(p_result->m_headerRegions);

    emit highlightCompleted();
}