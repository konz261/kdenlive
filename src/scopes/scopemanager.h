/***************************************************************************
 *   Copyright (C) 2011 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef SCOPEMANAGER_H
#define SCOPEMANAGER_H

#include "audioscopes/abstractaudioscopewidget.h"
#include "colorscopes/abstractgfxscopewidget.h"

#include <QtCore/QList>

class QDockWidget;
class AbstractRender;

/**
  \brief Manages communication between Scopes and Renderer

  The scope manager handles the data transfer between the active Renderer and
  all scopes that have been registered via ScopeManager::addScope(AbstractAudioScopeWidget, QDockWidget)
  or ScopeManager::addScope(AbstractGfxScopeWidget, QDockWidget). It checks whether the renderer really
  needs to send data (it does not, for example, if no scopes are visible).
  */
class ScopeManager : QObject
{
    Q_OBJECT

    struct GfxScopeData {
        AbstractGfxScopeWidget *scope;
        bool singleFrameRequested;
        GfxScopeData() {
            scope = NULL;
            singleFrameRequested = false;
        }
    };

    struct AudioScopeData {
        AbstractAudioScopeWidget *scope;
        bool singleFrameRequested;
        AudioScopeData() {
            scope = NULL;
            singleFrameRequested = false; 
        }
    };

public:
    explicit ScopeManager(QObject *parent = 0);

    /**
      Adds a scope and sets up signal/slot connections to ensure that the scope
      receives data when required.
      \see addScope(AbstractGfxScopeWidget, QDockWidget)
      */
    bool addScope(AbstractAudioScopeWidget *audioScope, QDockWidget *audioScopeWidget = NULL);
    /**
      \see addScope(AbstractAudioScopeWidget, QDockWidget)
      */
    bool addScope(AbstractGfxScopeWidget *colorScope, QDockWidget *colorScopeWidget = NULL);

private:
    QList<AudioScopeData> m_audioScopes;
    QList<GfxScopeData> m_colorScopes;

    AbstractRender *m_lastConnectedRenderer;

    QSignalMapper *m_signalMapper;

    /**
      Checks whether there is any scope accepting audio data, or if all of them are hidden
      or if auto refresh is disabled.
      \see imagesAcceptedByScopes(): Same for image data
      */
    bool audioAcceptedByScopes() const;
    /**
      \see audioAcceptedByScopes()
      */
    bool imagesAcceptedByScopes() const;


    /**
      Checks whether audio data is required, and notifies the renderer (enable or disable data sending).
      \see checkActiveAudioScopes() for image data
      */
    void checkActiveAudioScopes();
    /**
      Checks whether any scope accepts frames, and notifies the renderer.
      \see checkActiveAudioScopes() for audio data
      */
    void checkActiveColourScopes();

    /**
      Creates all the scopes in audioscopes/ and colorscopes/.
      New scopes are not detected automatically but have to be added.
     */
    void createScopes();

    /**
      Creates a dock for @param scopeWidget with the title @param title and
      adds it to the manager.
      @param scopeWidget has to be of type AbstractAudioScopeWidget or AbstractGfxScopeWidget (@see addScope).
     */
    template <class T> void createScopeDock(T *scopeWidget, const QString &title);

private slots:
    /**
      Updates the signal/slot connection since the active renderer has changed.
      */
    void slotUpdateActiveRenderer();
    /**
      The scope source was deleted, clear it.
      */
    void slotClearColorScopes();
    /**
      \see checkActiveAudioScopes()
      \see checkActiveColourScopes()
      */
    
    void slotCheckActiveScopes();
    void slotDistributeFrame(const QImage &image);
    void slotDistributeAudio(const QVector<qint16> &sampleData, int freq, int num_channels, int num_samples);
    /**
      Allows a scope to explicitly request a new frame, even if the scope's autoRefresh is disabled.
      */
    void slotRequestFrame(const QString &widgetName);


};

#endif // SCOPEMANAGER_H
