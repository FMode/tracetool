/* tracetool - a framework for tracing the execution of C++ programs
 * Copyright 2010-2016 froglogic GmbH
 *
 * This file is part of tracetool.
 *
 * tracetool is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tracetool is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with tracetool.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "searchwidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionFrame>
//#include <QStyleOptionFrameV2>
#include <QVBoxLayout>

#include <assert.h>

UnlabelledLineEdit::UnlabelledLineEdit( QWidget *parent )
{
}

void UnlabelledLineEdit::setPlaceholderText( const QString &placeholderText )
{
    m_placeholderText = placeholderText;
    update();
}

void UnlabelledLineEdit::paintEvent( QPaintEvent *e )
{
    QLineEdit::paintEvent( e );
    if ( !hasFocus() && text().isEmpty() ) {
        QPainter p( this );
        QFont f = font();
        f.setStyle( QFont::StyleItalic );
        p.setFont( f );
        p.setPen( QPen( Qt::gray ) );

        QStyleOptionHeader opt;
        //fmode initStyleOption( &opt );

        QRect r = style()->subElementRect( QStyle::SE_LineEditContents, &opt, this );

        p.drawText( r, Qt::AlignLeft | Qt::AlignVCenter, m_placeholderText );
    }
}

SearchWidget::SearchWidget( QWidget *parent )
    : QWidget( parent ),
    m_lineEdit( 0 ),
    m_buttonLayout( 0 )
{
    m_activeTraceKeyComboLabel = new QLabel( tr( "Trace Key:" ) );

    m_activeTraceKeyCombo = new QComboBox( this );
    m_activeTraceKeyCombo->setSizeAdjustPolicy( QComboBox::AdjustToContents );
    connect( m_activeTraceKeyCombo, SIGNAL( activated( const QString & ) ),
             this, SLOT( traceKeyChanged( const QString & ) ) );

    m_lineEdit = new UnlabelledLineEdit( this );
    connect( m_lineEdit, SIGNAL( textEdited( const QString & ) ),
             this, SLOT( termEdited( const QString & ) ) );
    m_lineEdit->setPlaceholderText( "Search trace data..." );

    m_strictMatch = new QRadioButton( tr( "Strict" ), this );
    m_strictMatch->setChecked( true );
    connect( m_strictMatch, SIGNAL( clicked() ),
             this, SLOT( emitSearchCriteria() ) );
    m_strictMatch->hide();
    m_wildcardMatch = new QRadioButton( tr( "Wildcard" ), this );
    connect( m_wildcardMatch, SIGNAL( clicked() ),
             this, SLOT( emitSearchCriteria() ) );
    m_wildcardMatch->hide();
    m_regexpMatch = new QRadioButton( tr( "RegExp" ), this );
    connect( m_regexpMatch, SIGNAL( clicked() ),
             this, SLOT( emitSearchCriteria() ) );
    m_regexpMatch->hide();

    m_buttonLayout = new QHBoxLayout;
    //fmode m_buttonLayout->setMargin( 0 );
    m_buttonLayout->setSpacing( 0 );

    m_modifierLayout = new QVBoxLayout;
    //fmode m_modifierLayout->setMargin( 0 );
    m_modifierLayout->setSpacing( 2 );
    m_modifierLayout->addWidget( m_strictMatch );
    m_modifierLayout->addWidget( m_wildcardMatch );
    m_modifierLayout->addWidget( m_regexpMatch );

    QGridLayout *layout = new QGridLayout( this );
    //fmode layout->setMargin( 0 );
    layout->addWidget( m_activeTraceKeyComboLabel, 0, 0 );
    layout->addWidget( m_activeTraceKeyCombo, 0, 1 );
    layout->addWidget( m_lineEdit, 0, 2 );
    layout->addLayout( m_buttonLayout, 1, 2 );
    layout->addLayout( m_modifierLayout, 0, 3, 2, 3 );
}

void SearchWidget::traceKeyChanged(const QString &key)
{
    emit activeTraceKeyChanged( key == tr( "<All keys>" ) ? ""
                                                          : key );
}

void SearchWidget::emitSearchCriteria()
{
    QStringList selectedFields;
    QList<QPushButton *>::ConstIterator it, end = m_fieldButtons.end();
    for ( it = m_fieldButtons.begin(); it != end; ++it ) {
        if ( ( *it )->isChecked() ) {
            selectedFields.append( ( *it )->text() );
        }
    }

    MatchType matchType;
    if ( m_strictMatch->isChecked() ) {
        matchType = StrictMatch;
    } else if ( m_wildcardMatch->isChecked() ) {
        matchType = WildcardMatch;
    } else if ( m_regexpMatch->isChecked() ) {
        matchType = RegExpMatch;
    } else {
        assert( !"Some match type radio button must be checked" );
    }

    emit searchCriteriaChanged( m_lineEdit->text(), selectedFields, matchType );
}

void SearchWidget::termEdited( const QString &newTerm )
{
    QList<QPushButton *>::ConstIterator it, end = m_fieldButtons.end();
    for ( it = m_fieldButtons.begin(); it != end; ++it ) {
        ( *it )->setVisible( !newTerm.isEmpty() );
    }
    m_strictMatch->setVisible( !newTerm.isEmpty() );
    m_wildcardMatch->setVisible( !newTerm.isEmpty() );
    m_regexpMatch->setVisible( !newTerm.isEmpty() );
    emitSearchCriteria();
}

void SearchWidget::setTraceKeys( const QStringList &keys )
{
    m_activeTraceKeyCombo->clear();
    m_activeTraceKeyCombo->addItem( tr( "<All keys>" ) );
    m_activeTraceKeyCombo->addItems( keys );
}

void SearchWidget::addTraceKeys( const QStringList &keys )
{
    QSet<QString> currentKeys;

    const int cnt = m_activeTraceKeyCombo->count();
    for ( int i = 0; i < cnt; ++i ) {
        currentKeys.insert( m_activeTraceKeyCombo->itemText( i ) );
    }

    /*QSet<QString> newKeys = QSet<QString>(keys.begin(), keys.end() );
    newKeys.subtract( currentKeys );*/

    for(const QString& key : keys)
    {
        if (!currentKeys.contains(key))
        {
            m_activeTraceKeyCombo->addItem(key);
        }
    }

}

void SearchWidget::setFields( const QStringList &fields )
{
    qDeleteAll( m_fieldButtons );
    m_fieldButtons.clear();

    int width = 0;

    QStringList::ConstIterator it, end = fields.end();
    for ( it = fields.begin(); it != end; ++it ) {
        QPushButton *fieldButton = new QPushButton( *it );
        connect( fieldButton, SIGNAL( clicked() ),
                 this, SLOT( emitSearchCriteria() ) );

        fieldButton->setCheckable( true );

        QFont f = fieldButton->font();
        f.setPointSize( ( f.pointSize() * 90 ) / 100 );
        fieldButton->setFont( f );

        m_buttonLayout->insertWidget( 0, fieldButton );

        m_fieldButtons.append( fieldButton );

        width += fieldButton->sizeHint().width();

        fieldButton->hide();
    }

    setMinimumWidth( m_activeTraceKeyComboLabel->sizeHint().width() +
                     m_activeTraceKeyCombo->sizeHint().width() +
                     qMax( width, m_lineEdit->minimumWidth() ) +
                     m_wildcardMatch->sizeHint().width() );
}

