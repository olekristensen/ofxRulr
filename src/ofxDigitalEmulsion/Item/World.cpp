#include "World.h"

#include "ofxCvGui.h"

using namespace ofxCvGui;

namespace ofxDigitalEmulsion {
	namespace Item {
		//-----------
		ofxCvGui::Controller * World::gui = 0;

		//-----------
		void World::setupGui(Controller & controller) {
			auto rootGroup = dynamic_pointer_cast<ofxCvGui::Panels::Groups::Grid>(controller.getRootGroup());
			if (rootGroup) {
				rootGroup->onBoundsChange += [rootGroup] (ofxCvGui::BoundsChangeArguments &) {
					const float inspectorWidth = 300.0f;
					vector<float> widths;
					widths.push_back(ofGetWindowWidth() - inspectorWidth);
					widths.push_back(inspectorWidth);
					rootGroup->setWidths(widths);
				};
			}

			auto gridGroup = MAKE(ofxCvGui::Panels::Groups::Grid);
			rootGroup->add(gridGroup);

			for(auto node : *this) {
				auto nodeView = node->getView();
				gridGroup->add(nodeView);

				nodeView->onMouse += [node] (MouseArguments & mouse) {
					if (mouse.isLocalPressed() && mouse.button == 0) {
						if (!ofxCvGui::isBeingInspected(* node)) {
							ofxCvGui::inspect(* node);
						}
					}
				};

				nodeView->onDraw += [node] (DrawArguments & drawArgs) {
					if (isBeingInspected(* node)) {
						ofPushStyle();
						ofSetColor(255);
						ofSetLineWidth(3.0f);
						ofNoFill();
						ofRect(drawArgs.localBounds);
						ofPopStyle();
					}
				};

				nodeView->setCaption(node->getTypeName());
			}

			auto inspector = ofxCvGui::Builder::makeInspector();
			rootGroup->add(inspector);

			Panels::Inspector::onClear += [] (ElementGroupPtr inspector) {
				inspector->add(Widgets::LiveValueHistory::make("Application fps [Hz]", [] () {
					return ofGetFrameRate();
				}, true));
				inspector->add(Widgets::Spacer::make());
			};

			World::gui = & controller;
		}

		//-----------
		void World::saveAll() const {
			for(auto node : * this) {
				node->save(node->getDefaultFilename());
			}
		}

		//-----------
		void World::loadAll() {
			for(auto node : * this) {
				node->load(node->getDefaultFilename());
			}
		}

		//-----------
		ofxCvGui::Controller & World::getGuiController() {
			if (World::gui) {
				return * World::gui;
			} else {
				OFXDIGITALEMULSION_FATAL << "No gui attached yet";
				return * (ofxCvGui::Controller *) 0;
			}
		}
	}
}