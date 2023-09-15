/* Modification */
// Taken straight from HPCC repo.
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/random-variable.h"
#include "ns3/qbb-net-device.h"
#include "ns3/ipv4-end-point.h"
#include "rdma-client.h"
#include "ns3/seq-ts-header.h"
#include <ns3/rdma-driver.h>
#include <stdlib.h>
#include <stdio.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RdmaClient");
NS_OBJECT_ENSURE_REGISTERED (RdmaClient);

TypeId
RdmaClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RdmaClient")
    .SetParent<Application> ()
    .AddConstructor<RdmaClient> ()
    .AddAttribute ("WriteSize",
                   "The number of bytes to write",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&RdmaClient::m_size),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("SourceIP",
                   "Source IP",
                   Ipv4AddressValue ("0.0.0.0"),
                   MakeIpv4AddressAccessor (&RdmaClient::m_sip),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("DestIP",
                   "Dest IP",
                   Ipv4AddressValue ("0.0.0.0"),
                   MakeIpv4AddressAccessor (&RdmaClient::m_dip),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("SourcePort",
                   "Source Port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_sport),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("DestPort",
                   "Dest Port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_dport),
                   MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("PriorityGroup", "The priority group of this flow",
				   UintegerValue (0),
				   MakeUintegerAccessor (&RdmaClient::m_pg),
				   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Window",
                   "Bound of on-the-fly packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_win),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BaseRtt",
                   "Base Rtt",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RdmaClient::m_baseRtt),
                   MakeUintegerChecker<uint64_t> ())
	.AddAttribute ("stopTime", "stopTime", TimeValue (Simulator::GetMaximumSimulationTime()),
				                      MakeTimeAccessor (&RdmaClient::stopTime),
				                      MakeTimeChecker ())

  ;
  return tid;
}

RdmaClient::RdmaClient ()
  : m_interMsgTime (0),
    m_msgSizePkts (0),
    m_totMsgCnt (0),
    m_avgMsgSizePkts (0),
    m_avgSize (0),
    m_avgTimeInterval (0),
    m_totalSizeBytes (0)
{
  NS_LOG_FUNCTION_NOARGS ();
}

RdmaClient::~RdmaClient ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void RdmaClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  m_dip = ip;
  m_dport = port;
}

void RdmaClient::SetLocal (Ipv4Address ip, uint16_t port)
{
  m_sip = ip;
  m_sport = port;
}

void RdmaClient::SetPG (uint16_t pg)
{
	m_pg = pg;
}

void RdmaClient::SetSize(uint64_t size){
	m_size = size;
}

void RdmaClient::Finish(){
	m_node->DeleteApplication(this);
}

void RdmaClient::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Application::DoDispose ();
}

void RdmaClient::SetWorkload (uint32_t avgSize,
                                   uint32_t avgTimeInterval,
                                   std::map<double,int> msgSizeCDF,
                                   double avgMsgSizePkts)
{
  NS_LOG_FUNCTION (this << avgMsgSizePkts);

  // Note the device 0 is usually loopback device
  // TODO: make it configurable
  m_msgSizeCDF = msgSizeCDF;
  m_avgSize = avgSize;
  m_avgTimeInterval = avgTimeInterval;
  m_avgMsgSizePkts = avgMsgSizePkts;

  m_interMsgTime = CreateObject<ExponentialRandomVariable> ();
  m_interMsgTime->SetAttribute ("Mean", DoubleValue (avgTimeInterval));

  m_msgSizePkts = CreateObject<UniformRandomVariable> ();
  m_msgSizePkts->SetAttribute ("Min", DoubleValue (0));
  m_msgSizePkts->SetAttribute ("Max", DoubleValue (1));
}
void RdmaClient::Start (Time start)
{
  NS_LOG_FUNCTION (this);

  SetStartTime (start);
  DoInitialize ();
}

void RdmaClient::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  // // get RDMA driver and add up queue pair
  // Ptr<Node> node = GetNode();
  // Ptr<RdmaDriver> rdma = node->GetObject<RdmaDriver>();
  ScheduleNextMessage ();
  // rdma->AddQueuePair(m_size, m_pg, m_sip, m_dip, m_sport, m_dport, m_win, m_baseRtt, MakeCallback(&RdmaClient::Finish, this),stopTime);
}

void RdmaClient::ScheduleNextMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  bool m_poissonFlow = true;
  if (Simulator::IsExpired (m_nextSendEvent))
    {
      if (m_poissonFlow)
        {
          m_nextSendEvent = Simulator::Schedule (NanoSeconds (m_interMsgTime->GetInteger ()),
                                                 &RdmaClient::SendMessage, this);
        }

      else
        {
          m_nextSendEvent = Simulator::Schedule (NanoSeconds (m_avgTimeInterval),
                                                 &RdmaClient::SendMessage, this);
        }
    }
  else
    {
      NS_LOG_WARN ("RdmaClient (" << this <<
                   ") tries to schedule the next msg before the previous one is sent!");
    }
}
uint32_t RdmaClient::GetNextMsgSizeFromDist ()
{
  NS_LOG_FUNCTION (this);

  int msgSizePkts = -1;
  double rndValue = m_msgSizePkts->GetValue (0.0, 1.0);
  for (auto it = m_msgSizeCDF.begin (); it != m_msgSizeCDF.end (); it++)
    {
      if (rndValue <= it->first)
        {
          msgSizePkts = it->second;
          break;
        }
    }

  NS_ASSERT (msgSizePkts >= 0);
  // Homa header can't handle msgs larger than 0xffff pkts
  // msgSizePkts = std::min(0xffff, msgSizePkts);

  uint32_t msgSizeBytes = msgSizePkts / m_avgMsgSizePkts * m_avgSize;

  return msgSizeBytes;

  // NOTE: If maxPayloadSize is not set, the generated messages will be
  //       slightly larger than the intended number of packets due to
  //       the addition of the protocol headers.
}
void RdmaClient::SendMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);

  /* Decide on the message size to send */
  uint32_t msgSizeBytes = GetNextMsgSizeFromDist ();

  /* Send the message */
  m_totMsgCnt += 1;
  m_totalSizeBytes += msgSizeBytes;
  // get RDMA driver and add up queue pair
  Ptr<Node> node = GetNode();
  Ptr<RdmaDriver> rdma = node->GetObject<RdmaDriver>();
  rdma->AddQueuePair(msgSizeBytes, m_pg, m_sip, m_dip, m_sport, m_dport, m_win, m_baseRtt, MakeCallback(&RdmaClient::Finish, this),stopTime);
  uint16_t m_maxMsgs = 0;
  bool m_microbenchExp = false;
  if (!m_microbenchExp && (m_maxMsgs == 0 || m_totMsgCnt < m_maxMsgs))
    {
      ScheduleNextMessage ();
    }
}

void RdmaClient::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();
  // TODO stop the queue pair
}

} // Namespace ns3
/* Modification */