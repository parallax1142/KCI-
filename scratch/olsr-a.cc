#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleOlsrExample");

#define ENABLE_INTERMEDIATE_LOGGING

void IntermediateRxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
#ifdef ENABLE_INTERMEDIATE_LOGGING
    NS_LOG_UNCOND("Intermediate node received one packet on interface " << interface << "!");
#endif
}

/* void PrintNodePositions(NodeContainer c)
{
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Ptr<MobilityModel> mobility = c.Get(i)->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        NS_LOG_UNCOND("Node " << i << " position: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z);
    }
    Simulator::Schedule(Seconds(5.0), &PrintNodePositions, c);
} */

int main(int argc, char *argv[])
{
    LogComponentEnable("SimpleOlsrExample", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentDisable("OnOffApplication", LOG_LEVEL_INFO);

    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(10); // Create 3 nodes

    // Set up WiFi devices and channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.0));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");

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

    // Set up a fixed mobility model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Node 0
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // Node 1
    positionAlloc->Add(Vector(20.0, 0.0, 0.0)); // Node 2
    positionAlloc->Add(Vector(30.0, 0.0, 0.0));  
    positionAlloc->Add(Vector(40.0, 0.0, 0.0)); 
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    positionAlloc->Add(Vector(60.0, 0.0, 0.0));
    positionAlloc->Add(Vector(70.0, 0.0, 0.0));
    positionAlloc->Add(Vector(80.0, 0.0, 0.0));
    positionAlloc->Add(Vector(90.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Schedule position logging
    // Simulator::Schedule(Seconds(5.0), &PrintNodePositions, c);

    // Create an OnOff application to send UDP datagrams from n0 to n2
    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(i.GetAddress(9), port));
    onoff1.SetConstantRate(DataRate("80Kb/s"));
    ApplicationContainer onOffApp1 = onoff1.Install(c.Get(0));
    onOffApp1.Start(Seconds(0.0));
    onOffApp1.Stop(Seconds(20.0));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));  // 패킷 크기를 1024 바이트로 설정

    // Create a PacketSink application to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(c.Get(9));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(21.0));

    Simulator::Stop(Seconds(30));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
