#include <QFileInfo>

#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>
#include <projectexplorer/buildconfiguration.h>

#include "OutputPane.h"
#include "OutputParser.h"
#include "PaneWidget.h"
#include "TestModel.h"
#include "ParseState.h"

using namespace QtcGtest::Internal;

OutputPane::OutputPane(QObject *parent) :
  IOutputPane(parent),
  parser_ (new OutputParser),
  model_ (new TestModel),
  state_ (new ParseState),
  widget_ (NULL),
  totalsLabel_ (new QLabel)
{
}

OutputPane::~OutputPane()
{
  delete totalsLabel_;
  delete model_;
  delete parser_;
  delete state_;
}

QWidget *OutputPane::outputWidget(QWidget *parent)
{
  Q_ASSERT (model_ != NULL);
  widget_ = new PaneWidget (model_, parent); // Can be only 1?
  connect (widget_.data (), SIGNAL (viewClicked (const QModelIndex&)),
           this, SLOT (handleViewClicked (const QModelIndex&)));
  return widget_.data ();
}

QList<QWidget *> OutputPane::toolBarWidgets() const
{
  QList<QWidget*> widgets;
  widgets << totalsLabel_;
  return widgets;
}

QString OutputPane::displayName() const
{
  return tr ("Google Test");
}

int OutputPane::priorityInStatusBar() const
{
  return 10;
}

void OutputPane::clearContents()
{
  model_->clear ();
}

void OutputPane::visibilityChanged(bool visible)
{
}

void OutputPane::setFocus()
{
  if (!widget_.isNull ())
  {
    widget_->setFocus ();
  }
}

bool OutputPane::hasFocus() const
{
  if (!widget_.isNull ())
  {
    return widget_->hasFocus ();
  }
  return false;
}

bool OutputPane::canFocus() const
{
  return (!widget_.isNull ());
}

bool OutputPane::canNavigate() const
{
  return true;
}

bool OutputPane::canNext() const
{
  Q_ASSERT (model_ != NULL);
  // Do not update value because Creator checks it BEFORE it can actually be updated.
  return (model_->errorCount () > 0);
}

bool OutputPane::canPrevious() const
{
  Q_ASSERT (model_ != NULL);
  // Do not update value because Creator checks it BEFORE it can actually be updated.
  return (model_->errorCount () > 0);
}

void OutputPane::goToNext()
{
  Q_ASSERT (!widget_.isNull ());
  QModelIndex currentIndex = widget_->currentIndex ();
  showError (model_->nextError (currentIndex));
}

void OutputPane::goToPrev()
{
  Q_ASSERT (!widget_.isNull ());
  QModelIndex currentIndex = widget_->currentIndex ();
  showError (model_->previousError (currentIndex));
}

void OutputPane::showError(const QModelIndex &errorIndex)
{
  if (!errorIndex.isValid ())
  {
    return;
  }
  widget_->setCurrentIndex (errorIndex);
  int row = errorIndex.row ();
  QString file = errorIndex.sibling (row, TestModel::ColumnFile).data ().toString ();
  int line = errorIndex.sibling (row, TestModel::ColumnLine).data ().toInt ();
  Core::EditorManager::openEditorAt (file, line);
}

void OutputPane::handleViewClicked(const QModelIndex &index)
{
  const TestModel* model = qobject_cast<const TestModel*> (index.model ());
  Q_ASSERT (model != NULL);
  TestModel::Type type = model->getType (index);
  if (type == TestModel::TypeDetailError)
  {
    showError (index);
  }
  else if (type == TestModel::TypeDetail)
  {
    QModelIndex previousError = model->previousError (index);
    if (previousError.isValid () && previousError.parent ().row () == index.parent ().row ())
    {
      showError (model->previousError (index));
    }
  }
}

void OutputPane::handleRunStart(ProjectExplorer::RunControl *control)
{
  state_->reset ();
  model_->clear ();
  state_->projectPath = control->runConfiguration ()->target ()->
                        activeBuildConfiguration ()->buildDirectory ().toString ();
  connect (control, SIGNAL (appendMessage(ProjectExplorer::RunControl *, const QString &, Utils::OutputFormat )),
           this, SLOT (parseMessage(ProjectExplorer::RunControl *, const QString &, Utils::OutputFormat )));

}

void OutputPane::handleRunFinish (ProjectExplorer::RunControl *control)
{
  if (state_->isGoogleTestRun)
  {
    totalsLabel_->setText (tr ("Total: passed %1 of %2 (%3 ms)").arg (
                             state_->passedTotalCount).arg (
                             state_->passedTotalCount +
                             state_->failedTotalCount).arg (state_->totalTime));
    popup (WithFocus);
  }
  disconnect (control, SIGNAL (appendMessage(ProjectExplorer::RunControl *, const QString &, Utils::OutputFormat )),
              this, SLOT (parseMessage(ProjectExplorer::RunControl *, const QString &, Utils::OutputFormat )));

}

void OutputPane::parseMessage(ProjectExplorer::RunControl *control, const QString &msg, Utils::OutputFormat format)
{
  Q_UNUSED (control);
  if (!(format == Utils::StdOutFormat || format == Utils::StdOutFormatSameLine))
  {
    return;
  }

  QStringList lines = msg.split (QLatin1Char ('\n'));
  foreach (const QString& line, lines)
  {
    if (line.trimmed ().isEmpty ())
    {
      continue;
    }
    if (!state_->isGoogleTestRun)
    {
      state_->isGoogleTestRun = parser_->isGoogleTestRun (line);
      continue;
    }
    parser_->parseMessage (line, *model_, *state_);
  }
}
