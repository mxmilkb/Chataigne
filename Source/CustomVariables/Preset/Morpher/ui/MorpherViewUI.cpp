/*
  ==============================================================================

    MorpherManagerUI.cpp
    Created: 11 Dec 2017 5:01:24pm
    Author:  Ben

  ==============================================================================
*/

#include "MorpherViewUI.h"
#include "CustomVariables/CVGroup.h"


MorpherPanel::MorpherPanel(StringRef contentName) :
	ShapeShifterContentComponent(contentName),
	currentMorpherUI(nullptr)
{
	InspectableSelectionManager::mainSelectionManager->addSelectionListener(this);

	CVGroup* group = InspectableSelectionManager::mainSelectionManager->getInspectableAs<CVGroup>();
	if (group != nullptr) setMorpher(group->morpher.get());
}

MorpherPanel::~MorpherPanel()
{
	setMorpher(nullptr);
	if(InspectableSelectionManager::mainSelectionManager != nullptr) InspectableSelectionManager::mainSelectionManager->removeSelectionListener(this);
}

void MorpherPanel::setMorpher(Morpher* m)
{
	if (currentMorpherUI != nullptr && currentMorpherUI->morpher == m) return;

	if (currentMorpherUI != nullptr)
	{
		currentMorpherUI->morpher->removeInspectableListener(this);
		removeChildComponent(currentMorpherUI.get());
		currentMorpherUI.reset();
	}

	if (m != nullptr)
	{
		currentMorpherUI.reset(new MorpherViewUI(m));
		addAndMakeVisible(currentMorpherUI.get());
		currentMorpherUI->morpher->addInspectableListener(this);
	}

	resized();
}

void MorpherPanel::paint(Graphics& g)
{
	if (currentMorpherUI == nullptr)
	{
		g.setColour(TEXTNAME_COLOR);
		g.setFont(20);
		g.drawText("Select a Custom Variable Group to edit its morpher", getLocalBounds().toFloat(), Justification::centred);
	}
}

void MorpherPanel::resized()
{
	if (currentMorpherUI != nullptr)
	{
		currentMorpherUI->setBounds(getLocalBounds());
	}
}

void MorpherPanel::inspectablesSelectionChanged()
{
	CVGroup * group = InspectableSelectionManager::mainSelectionManager->getInspectableAs<CVGroup>();
	if (group != nullptr) setMorpher(group->morpher.get());
}

void MorpherPanel::inspectableDestroyed(Inspectable* i)
{
	if (currentMorpherUI != nullptr && i == currentMorpherUI->morpher) setMorpher(nullptr);
}


// ---------------- MorpherViewUI

MorpherViewUI::MorpherViewUI(Morpher* morpher) :
	BaseManagerViewUI("Morpher", morpher->presetManager),
	morpher(morpher),
	mainTargetUI(&morpher->mainTarget),
	shouldRepaint(true)
{
	setInterceptsMouseClicks(true, true);
	setRepaintsOnMouseActivity(true);

	centerUIAroundPosition = true;
	updatePositionOnDragMove = true;
	
	setupBGImage();
	
	manager->addAsyncContainerListener(this);
	morpher->addAsyncCoalescedContainerListener(this);
	//manager->addMorpherListener(this);

	//mainTargetUI.setHandleColor(Colours::transparentBlack);
	//mainTargetUI.addItemUIListener(this);
	//mainTargetUI.removeBT->setVisible(false);
	//mainTargetUI.enabledBT->setVisible(false);
	mainTargetUI.setViewZoom(viewZoom);
	mainTargetUI.setAlwaysOnTop(true);
	addAndMakeVisible(mainTargetUI);

	acceptedDropTypes.add("MorphTarget");

	startTimerHz(25);

	addExistingItems();
}

MorpherViewUI::~MorpherViewUI()
{
	if(morpher != nullptr) morpher->removeAsyncContainerListener(this);
	manager->removeAsyncContainerListener(this);
}


void MorpherViewUI::paintBackground(Graphics& g)
{
	if (morpher == nullptr) return;

	//not paint checkerboard
	BaseManagerViewUI::paintBackground(g);

	//Voronoi
	switch (morpher->blendMode)
	{

	case Morpher::VORONOI:
	{
		if (morpher->diagram == nullptr || manager->items.size() == 0) break;

		const jcv_site* sites = jcv_diagram_get_sites(morpher->diagram.get());
		for (int i = 0; i < morpher->diagram->numsites; i++)
		{
			jcv_site s = sites[i];
			jcv_graphedge* e = s.edges;
			MorphTarget* target = morpher->getEnabledTargetAtIndex(s.index);
			Colour c = target->targetColor->getColor();
			float alpha = morpher->diagramOpacity->floatValue();
			//if (i == manager->curZoneIndex->intValue()) alpha = jmin<float>(alpha*2,1);
			g.setColour(c.withAlpha(alpha));

			Path p;
			while (e != nullptr)
			{
				Point<float> p1 = getPosInView(Point<float>(e->pos[0].x, e->pos[0].y));
				Point<float> p2 = getPosInView(Point<float>(e->pos[1].x, e->pos[1].y));


				if (p.getLength() == 0)
				{
					p.startNewSubPath(p1);
				}
				p.lineTo(p2);

				e = e->next;
			}
			p.closeSubPath();
			g.fillPath(p);
		}
	}
	default:
		break;
	}


	//BG Image
	g.setColour(Colours::white.withAlpha(morpher->bgOpacity->floatValue()));
	float tw = morpher->bgScale->floatValue();
	float th = bgImage.getHeight() * tw / bgImage.getWidth();
	Rectangle<float> r = getBoundsInView(Rectangle<float>(-tw / 2, -th / 2, tw, th));
	g.drawImage(bgImage, r);


	if (morpher->blendMode == Morpher::VORONOI)
	{
		if (morpher->showDebug->boolValue())
		{
			Point<float> mp = morpher->mainTarget.viewUIPosition->getPoint();

			int index = morpher->getSiteIndexForPoint(mp);

			if (index >= 0)
			{
				const jcv_site* sites = jcv_diagram_get_sites(morpher->diagram.get());
				jcv_site s = sites[index];

				//curZoneIndex->setValue(getSiteIndexForPoint(mp));

				//Compute direct site
				//MorphTarget * mt = manager->getEnabledTargetAtIndex(s.index);
				//float d = mp.getDistanceFrom(Point<float>(s.p.x, s.p.y));

				Point<float> mpVPos = getPosInView(mp);


				jcv_graphedge* edge = s.edges;
				Array<jcv_graphedge*> edges;

				Array<float> edgeDists;
				Array<float> edgeNeighbourDists;
				Array<jcv_site*> neighbourSites;
				Array<Line<float>> edgeLines;
				Array<Point<float>> neighboursViewPoints;
				Array<Point<float>> edgeViewPoints;

				g.setColour(Colours::white.withAlpha(.1f));

				Point<float> sVPos = getPosInView(Point<float>(s.p.x, s.p.y));
				g.drawLine(mpVPos.x, mpVPos.y, sVPos.x, sVPos.y);

				g.setColour(Colours::white.withAlpha(.6f));

				while (edge != nullptr)
				{
					jcv_site* ns = edge->neighbor;// e->sites[0]->index == s.index ? e->sites[1] : e->sites[0];

					if (ns == nullptr)
					{
						edge = edge->next;
						continue;
					}

					//DBG("Site " << s.index << " : Edge has sites " << e->sites[0]->index << "and " << e->sites[1]->index << ", chosen " << ns->index);
					//DBG("\t\t" << edge->pos[0].x << " / " << edge->pos[0].y << " / " << edge->pos[1].x << " / " << edge->pos[1].y);

					Line<float> line(Point<float>(edge->pos[0].x, edge->pos[0].y), Point<float>(edge->pos[1].x, edge->pos[1].y));
					Point<float> np;
					float distToEdge = line.getDistanceFromPoint(mp, np);
					//Point<float> sp;
					float distNeighbourToEdgePoint = np.getDistanceFrom(Point<float>(ns->p.x, ns->p.y));

					edges.add(edge);

					edgeDists.add(distToEdge);
					edgeNeighbourDists.add(distNeighbourToEdgePoint);
					neighbourSites.add(ns);
					edgeLines.add(line);

					edge = edge->next;

					Point<float> nsVPos = getPosInView(Point<float>(ns->p.x, ns->p.y));
					neighboursViewPoints.add(nsVPos);
					Point<float> npVPos = getPosInView(np);
					edgeViewPoints.add(npVPos);

					g.drawLine(mpVPos.x, mpVPos.y, nsVPos.x, nsVPos.y);
				}

				//DBG("Num edges to test " << edges.size());

				//Compute weight for each neighbour

				g.setColour(Colours::white.withAlpha(.6f));


				for (int i = 0; i < edges.size(); i++)
				{
					//float edgeDist = edgeDists[i];
					//float edgeNDist = edgeNeighbourDists[i];
					//float totalDist = edgeDist + edgeNDist;


					float minOtherEdgeDist = (float)INT32_MAX;
					jcv_graphedge* minEdge = nullptr;
					int minEdgeIndex = -1;

					for (int j = 0; j < edges.size(); j++)
					{
						if (i == j) continue;

						if (morpher->checkSitesAreNeighbours(edges[i]->neighbor, edges[j]->neighbor)) continue;

						float ed = edgeDists[j];
						if (ed < minOtherEdgeDist)
						{
							minOtherEdgeDist = ed;
							minEdge = edges[j];
							minEdgeIndex = j;
						}
					}

					if (minEdge != nullptr)
					{
						//float ratio = 1 - (edgeDist / (edgeDist + minOtherEdgeDist));
						g.setColour(Colours::orange);
						g.drawLine(mpVPos.x, mpVPos.y, edgeViewPoints[minEdgeIndex].x, edgeViewPoints[minEdgeIndex].y);
						g.setColour(Colours::yellow);
						g.drawLine(mpVPos.x, mpVPos.y, edgeViewPoints[i].x, edgeViewPoints[i].y);
						g.setColour(Colours::red);
						g.drawLine(edgeViewPoints[i].x, edgeViewPoints[i].y, neighboursViewPoints[i].x, neighboursViewPoints[i].y);
					}
					else
					{
						//float directDist = mp.getDistanceFrom(Point<float>(neighbourSites[i]->p.x, neighbourSites[i]->p.y)); //if we want to check direct distance instead of path to point
																															 //w = 1.0f / directDist;
						g.setColour(Colours::purple);
						g.drawLine(mpVPos.x, mpVPos.y, neighboursViewPoints[i].x, neighboursViewPoints[i].y);
					}
				}
			}
		}

		if (morpher->useAttraction->boolValue())
		{
			Point<float> mtp = morpher->mainTarget.viewUIPosition->getPoint();
			Point<float> mp = getPosInView(mtp);
			Point<float> tp = getPosInView(mtp + morpher->attractionDir);
			g.setColour(Colours::yellow);
			g.drawLine(mp.x, mp.y, tp.x, tp.y);
			//g.fillEllipse(tp.x - 5, tp.y - 5, 10, 10);
		}
	}



}

void MorpherViewUI::setupBGImage()
{
	bgImage.clear(bgImage.getBounds(), Colours::transparentWhite);
	File f = File(morpher->bgImagePath->stringValue());
	if (f.existsAsFile()) bgImage = ImageFileFormat::loadFrom(f);
	shouldRepaint = true;
}

void MorpherViewUI::resized()
{
	BaseManagerViewUI::resized();
	updateComponentViewPosition(&mainTargetUI, mainTargetUI.item->viewUIPosition->getPoint());
}

void MorpherViewUI::setViewZoom(float value)
{
	BaseManagerViewUI::setViewZoom(value);
	mainTargetUI.setViewZoom(value);
	updateComponentViewPosition(&mainTargetUI, mainTargetUI.item->viewUIPosition->getPoint());
}

void MorpherViewUI::mouseDown(const MouseEvent& e)
{
	BaseManagerViewUI::mouseDown(e);
	if (e.mods.isLeftButtonDown() && e.mods.isCommandDown())
	{
		morpher->mainTarget.viewUIPosition->setPoint(getViewMousePosition().toFloat() / viewZoom);
	}
}

void MorpherViewUI::mouseDrag(const MouseEvent& e)
{
	BaseManagerViewUI::mouseDrag(e);

	if (e.mods.isLeftButtonDown() && e.mods.isCommandDown())
	{
		morpher->mainTarget.viewUIPosition->setPoint(getViewMousePosition().toFloat() / viewZoom);
	}
}

void MorpherViewUI::mouseDoubleClick(const MouseEvent& e)
{
	BaseManagerViewUI::mouseDoubleClick(e);

	manager->addItem(getViewMousePosition().toFloat());
}

void MorpherViewUI::itemDragMove(const SourceDetails& details)
{
	BaseItemMinimalUI<MorphTarget>* bui = dynamic_cast<BaseItemMinimalUI<MorphTarget>*>(details.sourceComponent.get());

	if (bui != nullptr && bui == &mainTargetUI)
	{
		Point<int> p = getPositionFromDrag(bui, details);
		bui->item->viewUIPosition->setUndoablePoint(bui->item->viewUIPosition->getPoint(), p.toFloat());
		updateComponentViewPosition(bui, bui->item->viewUIPosition->getPoint());
		shouldRepaint = true;
	}

	BaseManagerViewUI::itemDragMove(details);
}

void MorpherViewUI::itemDropped(const SourceDetails& details)
{
	//String type = details.description.getProperty("type", "").toString();
	
	BaseItemMinimalUI<MorphTarget>* bui = dynamic_cast<BaseItemMinimalUI<MorphTarget>*>(details.sourceComponent.get());
	
	if(bui != nullptr && bui == &mainTargetUI)
	{
		Point<int> p = getPositionFromDrag(bui, details); 
		bui->item->viewUIPosition->setUndoablePoint(bui->item->viewUIPosition->getPoint(), p.toFloat());
		updateComponentViewPosition(bui, bui->item->viewUIPosition->getPoint());
	}

	BaseManagerViewUI::itemDropped(details);
	shouldRepaint = true;
}

void MorpherViewUI::newMessage(const ContainerAsyncEvent& e)
{
	switch (e.type)
	{
	case ContainerAsyncEvent::ControllableFeedbackUpdate:
		controllableFeedbackUpdateAsync(e.targetControllable);
		break;

	default:
		break;
	}
}

void MorpherViewUI::controllableFeedbackUpdateAsync(Controllable* c)
{
	if (c == morpher->bgImagePath)		 setupBGImage();
	else if (c == morpher->bgOpacity)	 shouldRepaint = true;
	else if (c == morpher->bgScale)		 shouldRepaint = true;
	else if (c == morpher->diagramOpacity)		 shouldRepaint = true;
	else if (c == morpher->mainTarget.viewUIPosition)
	{
		if (!mainTargetUI.isMouseOverOrDragging(true))
		{
			updateComponentViewPosition(&mainTargetUI,mainTargetUI.item->viewUIPosition->getPoint().toFloat());
			shouldRepaint = true;
		}
	}
	else if (MorphTarget * t = c->getParentAs<MorphTarget>())
	{
		if (c == t->viewUIPosition || c == t->targetColor || c == t->enabled) shouldRepaint = true;
		//
	}
}

void MorpherViewUI::timerCallback()
{
	if (shouldRepaint)
	{
		repaint();
		shouldRepaint = false;
	}
}
