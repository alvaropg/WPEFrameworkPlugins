#pragma once

#include "Module.h"
#include "Geometry.h"
#include <interfaces/json/JsonData_Streamer.h>

namespace WPEFramework {
namespace Plugin {

    class Streamer : public PluginHost::IPlugin, public PluginHost::IWeb, public PluginHost::JSONRPC {
    private:
        Streamer(const Streamer&) = delete;
        Streamer& operator=(const Streamer&) = delete;

        class StreamProxy {
        private:
            class StreamSink : public Exchange::IStream::ICallback {
            private:
                StreamSink() = delete;
                StreamSink(const StreamSink&) = delete;
                StreamSink& operator=(const StreamSink&) = delete;

            public:
                StreamSink(StreamProxy* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }
                virtual ~StreamSink()
                {
                }

            public:
                virtual void DRM(const uint32_t state) override
                {
                    _parent.DRM(state);
                }
                virtual void StateChange(const Exchange::IStream::state state) override
                {
                    _parent.StateChange(state);
                }

                BEGIN_INTERFACE_MAP(StreamSink)
                INTERFACE_ENTRY(Exchange::IStream::ICallback)
                END_INTERFACE_MAP

            private:
                StreamProxy& _parent;
            };

            class StreamControlSink : public Exchange::IStream::IControl::ICallback {
            private:
                StreamControlSink() = delete;
                StreamControlSink(const StreamControlSink&) = delete;
                StreamControlSink& operator=(const StreamControlSink&) = delete;

            public:
                StreamControlSink(StreamProxy* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }
                virtual ~StreamControlSink()
                {
                }

            public:
                virtual void TimeUpdate(const uint64_t position) override
                {
                    _parent.TimeUpdate(position);
                }

                BEGIN_INTERFACE_MAP(StreamControlSink)
                INTERFACE_ENTRY(Exchange::IStream::IControl::ICallback)
                END_INTERFACE_MAP

            private:
                StreamProxy& _parent;
            };

        public:
            StreamProxy() = delete;
            StreamProxy(const StreamProxy&) = delete;
            StreamProxy& operator= (const StreamProxy&) = delete;

            StreamProxy(Streamer& parent, const uint8_t index, Exchange::IStream* implementation) 
                : _parent(parent)
                , _index(index)
                , _implementation(implementation) 
                , _streamSink(this)
                , _streamControlSink(this) {
                ASSERT (_implementation != nullptr);
                _implementation->AddRef();
                _implementation->Callback(&_streamSink);
            }
            ~StreamProxy() {
                _implementation->Callback(nullptr);
                _implementation->Release();
            }

            Exchange::IStream* operator->() {
                return (_implementation);
            }
            const Exchange::IStream* operator->() const {
                return (_implementation);
            }

        private:
            void DRM(uint32_t state)
            {
                _parent.DRM(_index, state);
            }
            void StateChange(Exchange::IStream::state state)
            {
                _parent.StateChange(_index, state);
            }
            void TimeUpdate(const uint64_t position)
            {
                _parent.TimeUpdate(_index, position);
            }
 
        private:
            Streamer& _parent;
            uint8_t _index;
            Exchange::IStream* _implementation;
            Core::Sink<StreamSink> _streamSink;
            Core::Sink<StreamControlSink> _streamControlSink;
        };

        typedef std::map<uint8_t, StreamProxy> Streams;
        typedef std::map<uint8_t, Exchange::IStream::IControl*> Controls;

        class Config : public Core::JSON::Container {
        private:
            Config(const Config&);
            Config& operator=(const Config&);

        public:
            Config()
                : Core::JSON::Container()
                , OutOfProcess(true)
            {
                Add(_T("outofprocess"), &OutOfProcess);
            }
            ~Config()
            {
            }

        public:
            Core::JSON::Boolean OutOfProcess;
        };

    public:
        class Data : public Core::JSON::Container {
        private:
            Data(const Data&) = delete;
            Data& operator=(const Data&) = delete;

        public:
            Data()
                : Core::JSON::Container()
                , X()
                , Y()
                , Z()
                , Width()
                , Height()
                , Speed()
                , Position()
                , Type()
                , DRM()
                , State()
                , Url()
                , Begin()
                , End()
                , AbsoluteTime()
                , Id(~0)
                , Metadata(false)
                , Ids()
            {
                Add(_T("url"), &Url);
                Add(_T("x"), &X);
                Add(_T("y"), &Y);
                Add(_T("z"), &Z);
                Add(_T("width"), &Width);
                Add(_T("height"), &Height);
                Add(_T("speed"), &Speed);
                Add(_T("position"), &Position);
                Add(_T("type"), &Type);
                Add(_T("drm"), &DRM);
                Add(_T("state"), &State);
                Add(_T("begin"), &Begin);
                Add(_T("end"), &End);
                Add(_T("absolutetime"), &AbsoluteTime);
                Add(_T("id"), &Id);
                Add(_T("metadata"), &Metadata);
                Add(_T("ids"), &Ids);
            }
            ~Data()
            {
            }

        public:
            Core::JSON::DecUInt32 X;
            Core::JSON::DecUInt32 Y;
            Core::JSON::DecUInt32 Z;
            Core::JSON::DecUInt32 Width;
            Core::JSON::DecUInt32 Height;

            Core::JSON::DecSInt32 Speed;
            Core::JSON::DecUInt64 Position;
            Core::JSON::DecUInt32 Type;
            Core::JSON::DecUInt32 DRM;
            Core::JSON::DecUInt32 State;

            Core::JSON::String Url;
            Core::JSON::DecUInt64 Begin;
            Core::JSON::DecUInt64 End;
            Core::JSON::DecUInt64 AbsoluteTime;
            Core::JSON::DecUInt8 Id;

            Core::JSON::String Metadata;

            Core::JSON::ArrayType<Core::JSON::DecUInt8> Ids;
        };

    public:
#ifdef __WIN32__
#pragma warning(disable : 4355)
#endif
        Streamer()
            : _skipURL(0)
            , _pid(0)
            , _service(nullptr)
            , _player(nullptr)
            , _streams()
            , _controls()
        {
            RegisterAll();
        }
#ifdef __WIN32__
#pragma warning(default : 4355)
#endif
        virtual ~Streamer()
        {
            UnregisterAll();
        }

    public:
        BEGIN_INTERFACE_MAP(Streamer)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IWeb)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
        END_INTERFACE_MAP

    public:
        //  IPlugin methods
        // -------------------------------------------------------------------------------------------------------
        // First time initialization. Whenever a plugin is loaded, it is offered a Service object with relevant
        // information and services for this particular plugin. The Service object contains configuration information that
        // can be used to initialize the plugin correctly. If Initialization succeeds, return nothing (empty string)
        // If there is an error, return a string describing the issue why the initialisation failed.
        // The Service object is *NOT* reference counted, lifetime ends if the plugin is deactivated.
        // The lifetime of the Service object is guaranteed till the deinitialize method is called.
        virtual const string Initialize(PluginHost::IShell* service);

        // The plugin is unloaded from WPEFramework. This is call allows the module to notify clients
        // or to persist information if needed. After this call the plugin will unlink from the service path
        // and be deactivated. The Service object is the same as passed in during the Initialize.
        // After theis call, the lifetime of the Service object ends.
        virtual void Deinitialize(PluginHost::IShell* service);

        // Returns an interface to a JSON struct that can be used to return specific metadata information with respect
        // to this plugin. This Metadata can be used by the MetData plugin to publish this information to the ouside world.
        virtual string Information() const;

        //  IWeb methods
        // -------------------------------------------------------------------------------------------------------
        virtual void Inbound(Web::Request& request);
        virtual Core::ProxyType<Web::Response> Process(const Web::Request& request);
        PluginHost::IShell* GetService() { return _service; }

    private:
        Core::ProxyType<Web::Response> GetMethod(Core::TextSegmentIterator& index);
        Core::ProxyType<Web::Response> PutMethod(Core::TextSegmentIterator& index, const Web::Request& request);
        Core::ProxyType<Web::Response> PostMethod(Core::TextSegmentIterator& index, const Web::Request& request);
        Core::ProxyType<Web::Response> DeleteMethod(Core::TextSegmentIterator& index);
        void Deactivated(RPC::IRemoteProcess* process);

        void DRM(const uint8_t index, uint32_t state)
        {
            string stateText (_T("playready"));
            _service->Notify(_T("{ \"id\": ") + 
                             Core::NumberType<uint8_t>(index).Text() + 
                             _T(", \"drm\": \"") + 
                             stateText + 
                             _T("\" }"));
        }
        void StateChange(const uint8_t index, Exchange::IStream::state state)
        {
            TRACE(Trace::Information, (_T("Stream [%d] moved state: [%s]"), index, Core::EnumerateType<Exchange::IStream::state>(state).Data()));

            _service->Notify(_T("{ \"id\": ") + 
                             Core::NumberType<uint8_t>(index).Text() + 
                             _T(", \"stream\": \"") + 
                             Core::EnumerateType<Exchange::IStream::state>(state).Data() + 
                             _T("\" }"));
        }
        void TimeUpdate(const uint8_t index, const uint64_t position)
        {
            _service->Notify(_T("{ \"id\": ") + 
                             Core::NumberType<uint8_t>(index).Text() + 
                             _T(", \"time\": ") + 
                             Core::NumberType<uint64_t>(position).Text()+ _T(" }"));
        }

        // JsonRpc
        void RegisterAll();
        void UnregisterAll();
        uint32_t endpoint_status(const JsonData::Streamer::StatusParamsInfo& params, JsonData::Streamer::StatusResultData& response);
        uint32_t endpoint_create(const JsonData::Streamer::CreateParamsData& params, Core::JSON::DecUInt8& response);
        uint32_t endpoint_destroy(const JsonData::Streamer::StatusParamsInfo& params);
        uint32_t endpoint_load(const JsonData::Streamer::LoadParamsData& params);
        uint32_t endpoint_attach(const JsonData::Streamer::StatusParamsInfo& params);
        uint32_t endpoint_detach(const JsonData::Streamer::StatusParamsInfo& params);
        uint32_t endpoint_speeds(const JsonData::Streamer::StatusParamsInfo& params, Core::JSON::ArrayType<Core::JSON::DecSInt32>& response);
        uint32_t set_speed(const JsonData::Streamer::SpeedParamsData& params);
        uint32_t get_speed(JsonData::Streamer::SpeedParamsData& params) const;
        uint32_t set_position(const JsonData::Streamer::PositionParamsData& params);
        uint32_t get_position(JsonData::Streamer::PositionParamsData& params) const;
        uint32_t set_window(const JsonData::Streamer::WindowParamsData& params);
        uint32_t get_window(JsonData::Streamer::WindowParamsData& params) const;
        uint32_t endpoint_streams(Core::JSON::ArrayType<Core::JSON::DecUInt32>& response);
        uint32_t endpoint_type(const JsonData::Streamer::StatusParamsInfo& params, JsonData::Streamer::TypeResultData& response);
        uint32_t endpoint_drm(const JsonData::Streamer::StatusParamsInfo& params, JsonData::Streamer::DrmResultInfo& response);
        uint32_t endpoint_state(const JsonData::Streamer::StatusParamsInfo& params, JsonData::Streamer::StateResultInfo& response);
        void event_statechange(const string& id, const JsonData::Streamer::StateType& state);
        void event_drmchange(const string& id, const JsonData::Streamer::DrmType& drm);
        void event_timeupdate(const string& id, const uint64_t& time);

    private:
        uint32_t _skipURL;
        uint32_t _pid;
        PluginHost::IShell* _service;

        Exchange::IPlayer* _player;

        // Stream and StreamControl holding areas for the RESTFull API.
        Streams _streams;
        Controls _controls;
    };
} //namespace Plugin
} //namespace WPEFramework
