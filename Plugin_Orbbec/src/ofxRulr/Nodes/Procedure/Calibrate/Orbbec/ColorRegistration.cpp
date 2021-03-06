#include "pch_Plugin_Orbbec.h"
#include "ColorRegistration.h"

#include "ofxRulr/Nodes/Item/Orbbec/Color.h"
#include "ofxRulr/Nodes/Item/Orbbec/Infrared.h"
#include "ofxRulr/Nodes/Item/Board.h"

using namespace ofxCv;

namespace ofxRulr {
	namespace Nodes {
		namespace Procedure {
			namespace Calibrate {
				namespace Orbbec {
					//----------
					ColorRegistration::ColorRegistration() {
						RULR_NODE_INIT_LISTENER;
					}

					//----------
					string ColorRegistration::getTypeName() const {
						return "Procedure::Calibrate::Orbbec::ColorRegistration";
					}

					//----------
					void ColorRegistration::init() {
						RULR_NODE_UPDATE_LISTENER;
						RULR_NODE_INSPECTOR_LISTENER;
						RULR_NODE_SERIALIZATION_LISTENERS;

						this->addInput<Nodes::Item::Orbbec::Device>();
						this->addInput<Nodes::Item::Orbbec::Color>();
						this->addInput<Nodes::Item::Orbbec::Infrared>();
						this->addInput<Nodes::Item::Board>();

						this->panelStrip = make_shared<ofxCvGui::Panels::Groups::Strip>();
						{
							auto irPreview = make_shared<ofxCvGui::Panels::Texture>(this->irPreview);
							irPreview->setCaption("IR");
							irPreview->onDrawImage += [this](ofxCvGui::DrawImageArguments & args) {
								for (const auto & capture : this->captures) {
									ofxCv::drawCorners(capture.irImagePoints);
								}
							};
							this->panelStrip->add(irPreview);
						}
						{
							auto colorPreview = make_shared<ofxCvGui::Panels::Texture>(this->colorPreview);
							colorPreview->setCaption("Color");
							colorPreview->onDrawImage += [this](ofxCvGui::DrawImageArguments & args) {
								for (const auto & capture : this->captures) {
									ofxCv::drawCorners(capture.colorImagePoints);
								}
							};
							this->panelStrip->add(colorPreview);
						}

						this->manageParameters(this->parameters);
					}

					//----------
					void ColorRegistration::update() {
						auto device = this->getInput<Item::Orbbec::Device>();
						if (device) {
							{
								auto irStream = device->getDevice()->get<ofxOrbbec::Streams::Infrared>(false);
								if (irStream) {
									auto irFrame16 = irStream->getPixels();
									ofPixels irFrame;
									irFrame.allocate(irFrame16.getWidth(), irFrame16.getHeight(), 1);
									auto in = irFrame16.getData();
									auto out = irFrame.getData();
									for (int i = 0; i < irFrame.size(); i++) {
										*out++ = ofClamp(float(*in++) * this->parameters.irExposure, 0, 255);
									}
									this->irPreview.loadData(irFrame);
								}
							}
							{
								auto colorStream = device->getDevice()->get<ofxOrbbec::Streams::Color>(false);
								if (colorStream) {
									this->colorPreview = colorStream->getTexture();
								}
							}
						}
					}

					//----------
					void ColorRegistration::populateInspector(ofxCvGui::InspectArguments & args) {
						auto inspector = args.inspector;
						inspector->addLiveValue<size_t>("Capture count:", [this]() {
							return this->captures.size();
						});
						inspector->addButton("Add capture", [this]() {
							try {
								this->addCapture();
							}
							RULR_CATCH_ALL_TO_ERROR;
						}, ' ');
						inspector->addButton("Clear all captures", [this]() {
							this->captures.clear();
						});
						inspector->addButton("Clear last capture", [this]() {
							if (!this->captures.empty()) {
								auto last = this->captures.end();
								last--;
								this->captures.erase(last);
							}
						});
						{
							auto calibrateButton = make_shared<ofxCvGui::Widgets::Button>("Calibrate", [this]() {
								try {
									this->calibrate();
								}
								RULR_CATCH_ALL_TO_ALERT;
							}, OF_KEY_RETURN);
							calibrateButton->setHeight(100.0f);
							inspector->add(calibrateButton);
						}
						inspector->addLiveValue<float>("Reprojection error", [this]() {
							return this->reprojectionError;
						});

					}

					//----------
					void ColorRegistration::serialize(Json::Value & json) {
						int index = 0;
						auto & capturesJson = json["captures"];
						for (const auto & capture : this->captures) {
							auto & captureJson = capturesJson[index++];
							

							{
								int pointIndex = 0;
								for (auto & point : capture.irImagePoints) {
									captureJson["irImagePoints"][pointIndex++] << point;
								}
							}
							{
								int pointIndex = 0;
								for (auto & point : capture.colorImagePoints) {
									captureJson["colorImagePoints"][pointIndex++] << point;
								}
							}
							{
								int pointIndex = 0;
								for (auto & point : capture.boardPoints) {
									captureJson["boardPoints"][pointIndex++] << point;
								}
							}
						}
						Utils::Serializable::serialize(json, this->reprojectionError);
					}

					//----------
					void ColorRegistration::deserialize(const Json::Value & json) {
						this->captures.clear();
						const auto & capturesJson = json["captures"];
						for (const auto & captureJson : capturesJson) {
							Capture capture;
							for (auto & pointJson : captureJson["irImagePoints"]) {
								ofVec2f point;
								pointJson >> point;
								capture.irImagePoints.push_back(point);
							}
							for (auto & pointJson : captureJson["colorImagePoints"]) {
								ofVec2f point;
								pointJson >> point;
								capture.colorImagePoints.push_back(point);
							}
							for (auto & pointJson : captureJson["boardPoints"]) {
								ofVec3f point;
								pointJson >> point;
								capture.boardPoints.push_back(point);
							}
							this->captures.push_back(capture);
						}
						Utils::Serializable::deserialize(json, this->reprojectionError);
					}

					//----------
					void ColorRegistration::addCapture() {
						this->throwIfMissingAnyConnection();

						auto deviceNode = this->getInput<Item::Orbbec::Device>();
						auto colorNode = this->getInput<Item::Orbbec::Color>();
						auto infraredNode = this->getInput<Item::Orbbec::Infrared>();
						auto boardNode = this->getInput<Item::Board>();
						auto device = deviceNode->getDevice();

						//we'll mess around with its streams, let it rebuild itself
						deviceNode->streamsNeedRebuilding = true;

						auto captureNewFrame = [device]() {
							device->update();
							auto startTime = chrono::system_clock::now();
							while (!device->isFrameNew()) {
								if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startTime).count() > 2) {
									throw(ofxRulr::Exception("Timeout waiting for device frame"));
								}
								ofSleepMillis(1);
								glfwPollEvents();
								device->update();
							}
						};

						Capture capture;
						capture.boardPoints = toOf(boardNode->getObjectPoints());

						ofPixels irFrame;
						ofPixels colorFrame;
						{
							ofxRulr::Utils::ScopedProcess scopedProcess("Capturing images");

							//--
							//get the IR frame
							//--
							//
							{
								device->closeColor();
								device->initInfrared();

								captureNewFrame();

								//capture ir frame and scale to 8 bit
								auto irFrame16 = device->getInfrared()->getPixels();
								if (!irFrame16.isAllocated()) {
									throw(ofxRulr::Exception("No IR frame captured"));
								}
								irFrame.allocate(irFrame16.getWidth(), irFrame16.getHeight(), 1);
								auto in = irFrame16.getData();
								auto out = irFrame.getData();
								for (int i = 0; i < irFrame.size(); i++) {
									*out++ = ofClamp(float(*in++) * this->parameters.irExposure, 0, 255);
								}
							}
							//
							//--

							ofSleepMillis(1000);

							//--
							//get the color frame
							//--
							//
							{
								device->closeInfrared();
								device->initColor();

								captureNewFrame();

								colorFrame = device->getColor()->getPixels();
								if (!colorFrame.isAllocated()) {
									throw(ofxRulr::Exception("No color frame captured"));
								}
							}
							//
							//--

							scopedProcess.end();
						}

						this->irPreview.loadData(irFrame);
						this->colorPreview.loadData(colorFrame);

						{
							Utils::ScopedProcess scopedProcess("Finding boards in images");
							if (!boardNode->findBoard(toCv(irFrame), toCv(capture.irImagePoints), FindBoardMode::Optimized)) {
								throw(ofxRulr::Exception("Board not seen in IR image"));
							}
							if (!boardNode->findBoard(toCv(colorFrame), toCv(capture.colorImagePoints), FindBoardMode::Optimized)) {
								throw(ofxRulr::Exception("Board not seen in IR image"));
							}
							scopedProcess.end();
						}
						this->captures.push_back(capture);						
					}

					//----------
					void ColorRegistration::calibrate() {
						this->throwIfMissingAnyConnection();

						Utils::ScopedProcess scopedProcess("Calibrate");
						vector<vector<Point3f>> objectPoints;
						vector<vector<Point2f>> irImagePoints;
						vector<vector<Point2f>> colorImagePoints;

						if (this->captures.empty()) {
							throw(ofxRulr::Exception("No captures. Please capture some boards first!"));
						}
						for (auto capture : this->captures) {
							objectPoints.push_back(toCv(capture.boardPoints));
							irImagePoints.push_back(toCv(capture.irImagePoints));
							colorImagePoints.push_back(toCv(capture.colorImagePoints));
						}
						
						auto irNode = this->getInput<Item::Orbbec::Infrared>();
						auto colorNode = this->getInput<Item::Orbbec::Color>();

						auto irCameraMatrix = irNode->getCameraMatrix();
						auto irDistortionCoefficients = irNode->getDistortionCoefficients();

						auto colorCameraMatrix = colorNode->getCameraMatrix();
						auto colorDistortionCoefficients = colorNode->getDistortionCoefficients();

						//first let's calibrate the cameras
						{
							int flags = CV_CALIB_USE_INTRINSIC_GUESS | CV_CALIB_FIX_K5 | CV_CALIB_FIX_K6 | this->parameters.allowDistortion
								? 0
								: CV_CALIB_USE_INTRINSIC_GUESS | CV_CALIB_FIX_K1 | CV_CALIB_FIX_K2 | CV_CALIB_FIX_K3 | CV_CALIB_FIX_K4 | CV_CALIB_ZERO_TANGENT_DIST;

							vector<cv::Mat> rotations, translations;
							cv::calibrateCamera(objectPoints
								, colorImagePoints
								, colorNode->getSize()
								, colorCameraMatrix
								, colorDistortionCoefficients
								, rotations
								, translations
								, flags);
							colorNode->setIntrinsics(colorCameraMatrix, colorDistortionCoefficients);

							if (this->parameters.calibrateIRIntrinsics) {
								cv::calibrateCamera(objectPoints
									, irImagePoints
									, irNode->getSize()
									, irCameraMatrix
									, irDistortionCoefficients
									, rotations
									, translations
									, flags);

								irNode->setIntrinsics(irCameraMatrix, irDistortionCoefficients);
							}
						}
						
						cv::Mat rotation, translation;
						cv::Mat essentialMatrx, fundamentalMatrix;

						//then let's stereo calibrate between the 2 of them
						this->reprojectionError = cv::stereoCalibrate(objectPoints
							, irImagePoints
							, colorImagePoints
							, irCameraMatrix
							, irDistortionCoefficients
							, colorCameraMatrix
							, colorDistortionCoefficients
							, irNode->getSize()
							, rotation
							, translation
							, essentialMatrx
							, fundamentalMatrix
							, TermCriteria(CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 100, 1e-5)
							, CV_CALIB_FIX_INTRINSIC | CV_CALIB_USE_INTRINSIC_GUESS);

						colorNode->setExtrinsics(rotation, translation);

						scopedProcess.end();
					}

					//----------
					ofxCvGui::PanelPtr ColorRegistration::getPanel() {
						return this->panelStrip;
					}
				}
			}
		}
	}
}