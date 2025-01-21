#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-routing-protocol.h"

#include <random>

// 2개의 노드를 멀리 떨어뜨린다.
// 약 3초 뒤 통신 범위에 들어간다.
// 3초 뒤에 바로 통신이 되는지, TC timer가 끝난 후에 통신이 되는지 test

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleOlsrExample");

#define ENABLE_INTERMEDIATE_LOGGING

void IntermediateRxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
#ifdef ENABLE_INTERMEDIATE_LOGGING
    NS_LOG_UNCOND("Intermediate node received one packet on interface " << interface << "!");
#endif
}

void PrintNodePositions(NodeContainer c)
{
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Ptr<MobilityModel> mobility = c.Get(i)->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        NS_LOG_UNCOND("Node " << i << " position: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z);
    }
    Simulator::Schedule(Seconds(5.0), &PrintNodePositions, c);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("SimpleOlsrExample", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO); // PacketSink 로깅 활성화
    LogComponentDisable("OnOffApplication", LOG_LEVEL_INFO); // OnOff 로깅 비활성화

    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(2); 

    // Set up WiFi devices and channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // 전파 손실 측정 // 로그 거리 전파 손실 모델 // 송신점에서의 거리의 로그에 비례하여 감소 // Exponent 경로 손실 지수(신호 감쇠 정도)
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(1.8));  // 일반적인 실외 환경 2~4 -> FANET은 장애물이 없으므로 더 적은 값이 좋을듯

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager"); // Wi-Fi 속도 제어 // 이상적인 조건(채널 상태 정보 완벽) IdealWifiManager


    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    // Enable OLSR
    NS_LOG_INFO("Enabling OLSR Routing.");
    OlsrHelper olsr;

    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(c);

    // Assign IP addresses
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(c);

    Ptr<Node> node0 = c.Get(0);
    Ptr<Node> node1 = c.Get(1);

    Ptr<ConstantVelocityMobilityModel> loc1 = node0->GetObject<ConstantVelocityMobilityModel>();
    loc1->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<ConstantVelocityMobilityModel> loc2 = node1->GetObject<ConstantVelocityMobilityModel>();
    loc2->SetPosition(Vector(1500.0, 0.0, 0.0));  // 통신 가능 거리 700

    loc2->SetVelocity(Vector(-300.0, 0.0, 0.0));
    Simulator::Schedule(Seconds(4.0), &ConstantVelocityMobilityModel::SetVelocity, loc2, Vector(0.0, 0.0, 0.0));

    // Schedule position logging
    Simulator::Schedule(Seconds(3.0), &PrintNodePositions, c);

    // Create an OnOff application to send UDP datagrams from n0 to n2
    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)

    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
    onoff1.SetConstantRate(DataRate("80Kb/s"));

    ApplicationContainer onOffApp1 = onoff1.Install(c.Get(0));
    onOffApp1.Start(Seconds(0.0));
    onOffApp1.Stop(Seconds(20.0));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));  // 패킷 크기를 1024 바이트로 설정


    // Create a PacketSink application to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(c.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(21.0));

    Simulator::Stop(Seconds(30));
    

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}