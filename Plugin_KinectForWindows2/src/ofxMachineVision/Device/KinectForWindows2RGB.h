#pragma once

#include "ofxKinectForWindows2.h"
#include "../../../addons/ofxMachineVision/src/ofxMachineVision/Device/Updating.h"

namespace ofxMachineVision {
	namespace Device {
		class KinectForWindows2RGB : public Updating {
		public:
			class InitialisationSettings : public Base::InitialisationSettings {
			public:
				InitialisationSettings() {
					this->clear(); // remove device ID as a setting
					this->add(this->color);
				}

				ofParameter<bool> color{ "Color", true };
			};

			KinectForWindows2RGB();
			string getTypeName() const override;
			shared_ptr<Base::InitialisationSettings> getDefaultSettings() override {
				return make_shared<InitialisationSettings>();
			}
			Specification open(shared_ptr<Base::InitialisationSettings> = nullptr) override;
			void close() override;

			void updateIsFrameNew() override;
			bool isFrameNew() override;
			shared_ptr<Frame> getFrame() override;

			shared_ptr<ofxKinectForWindows2::Device> getKinect();
		protected:
			int frameIndex;
			bool markFrameNew;
			ofxMachineVision::Microseconds openTime;
			shared_ptr<ofxKinectForWindows2::Device> kinect;
			bool useColor = true;
		};
	}
}