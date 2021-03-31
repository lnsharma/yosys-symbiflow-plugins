/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2021  Lalit Sharma <lsharma@quicklogic.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/sigtools.h"
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct QL_inoutPass : public Pass {

    struct InoutPortInfo {
        std::string inoutWireName;		   // inout wire name present in design
        RTLIL::Wire *newOutPort;		   // Output wire name created to replace inout
        RTLIL::Wire *newEnablePort;		   // Enable wire name created to replace inout
        std::vector<std::string> inoutConnSigName; // connected signal name which are driven by this wire

        InoutPortInfo(const std::string &inoutWire, RTLIL::Wire *outPort, RTLIL::Wire *enablePort,
                const std::vector<std::string> sigVec = std::vector<std::string>())
            : inoutWireName(inoutWire), newOutPort(outPort), newEnablePort(enablePort), inoutConnSigName(sigVec)
        {
        }
    };

    QL_inoutPass() : Pass("ql_inout", "replace inout ports with inout_$in, inout_$out and inout_$en") {}
    void help() override
    {
        log("\n");
        log("    ql_inout [options] [selection]\n");
        log("\n");
        log("\"Replace\" inout ports with input, output and enable ports, if possible.\n");
        log("\n");
    }

    SigMap assign_map;
    void execute(std::vector<std::string> args, RTLIL::Design *design) override
    {
        log_header(design, "Executing QL_INOUT pass (replace inout ports with input, output and enable ports).\n");

        extra_args(args, 1, design);

        bool keep_running = true;

        while (keep_running) {
            keep_running = false;

            for (auto module : design->selected_modules()) {
                SigMap sigmap(module);

                // key is inout port name, value is object containing its information
                std::map<std::string, InoutPortInfo> inoutMap;

                // find all inout ports
                std::vector<Wire *> inoutWireVec;
                for (auto *wire : module->wires()) {
                    if (wire->port_input && wire->port_output) {
                        inoutWireVec.push_back(wire);
                    }
                }

                for (auto *wire : inoutWireVec) {
                    if (!wire)
                        continue;

                    string currWireName(wire->name.c_str());
                    string wireOutName = currWireName + "_$out";
                    string wireEnName = currWireName + "_$en";

                    Wire *outWire = module->addWire(wireOutName, wire);
                    if (outWire) {
                        outWire->port_output = true;
                        outWire->port_input = false;
                    }
                    Wire *enWire = module->addWire(wireEnName, wire);
                    if (enWire) {
                        enWire->port_output = true;
                        enWire->port_input = false;
                    }

                    InoutPortInfo obj(currWireName, outWire, enWire);
                    inoutMap.emplace(currWireName, obj);
                }

                // key: connected signal name to inout port
                // value: its InoutPortInfo object
                std::map<std::string, const InoutPortInfo &> connSigInoutMap;
                // gather all connections to inout ports
                for (auto connection : module->connections()) {
                    std::string wireName(log_signal(connection.first));
                    auto pos = inoutMap.find(wireName);
                    if (pos != inoutMap.end()) {
                        auto &currInoutObj = pos->second;
                        auto &inputConnSigNameVec = currInoutObj.inoutConnSigName;
                        inputConnSigNameVec.push_back(log_signal(connection.second));
                        connSigInoutMap.emplace(log_signal(connection.second), currInoutObj);
                    }
                }

                // key: ouput signal of a cell, value: enable signal of a cell
                std::map<RTLIL::IdString, RTLIL::SigSpec> enSigMap;

                // find tribuf cells
                for (auto cell : module->cells()) {

                    for (auto &conn : cell->connections()) {

                        if (conn.first == ID::Y && cell->type.in(ID($mux), ID($pmux), ID($_MUX_), ID($_TBUF_), ID($tribuf))) {
                            bool tribuf = cell->type.in(ID($_TBUF_), ID($tribuf));

                            if (!tribuf) {
                                for (auto &c : cell->connections()) {
                                    if (!c.first.in(ID::A, ID::B))
                                        continue;
                                    for (auto b : sigmap(c.second))
                                        if (b == State::Sz)
                                            tribuf = true;
                                }
                            }

                            if (tribuf) {
                                RTLIL::SigSpec sig_s = assign_map(cell->getPort(ID::S));
                                RTLIL::SigSpec sig_y = assign_map(cell->getPort(ID::Y));

                                enSigMap.emplace(log_signal(sig_y), sig_s);
                            }
                        }
                    }
                }

                // Replace inout wires with bidir_$in (if bidir act as input) in the connection, bidir_$out (if bidir act
                // as output) or bidir_$en (if bidir act as enable)
                for (auto *wire : inoutWireVec) {
                    if (!wire)
                        continue;

                    std::string currWire(wire->name.c_str());

                    vector<SigSig> new_connections;

                    auto &connectionVec = module->connections();

                    auto pos = inoutMap.find(currWire);

                    for (auto &currConn : connectionVec) {
                        if (string(currConn.first.as_wire()->name.c_str()) == currWire) {
                            if (pos != inoutMap.end()) {
                                SigSig new_conn;
                                auto &currInoutObj = pos->second;
                                new_conn.first.append(currInoutObj.newOutPort);
                                new_conn.second.append(currConn.second);
                                new_connections.push_back(new_conn);
                            }
                        } else {
                            SigSig new_conn;
                            new_conn.first.append(currConn.first);
                            new_conn.second.append(currConn.second);
                            new_connections.push_back(new_conn);
                        }
                    }

                    if (pos == inoutMap.end())
                        continue;

                    auto &currInoutObj = pos->second;
                    auto &inputConnSigNameVec = currInoutObj.inoutConnSigName;
                    for (auto sigName : inputConnSigNameVec) {
                        auto connPos = enSigMap.find(sigName);
                        if (connPos != enSigMap.end()) {
                            auto connSig = connPos->second;

                            RTLIL::SigSpec sig_en = assign_map(connSig);
                            for (int i = 0; i < abs(wire->width - wire->start_offset - 1); ++i) {
                                sig_en.append(assign_map(connSig));
                            }

                            if (currInoutObj.newEnablePort) {
                                SigSig new_conn;
                                string enWireName(currInoutObj.newEnablePort->name.c_str());
                                new_conn.first.append(assign_map(currInoutObj.newEnablePort));
                                new_conn.second.append(sig_en);
                                new_connections.push_back(new_conn);
                            }
                        }
                    }
                    module->new_connections(new_connections);

                    string wireInName = currWire + "_$in";
                    wire->port_output = false;
                    module->rename(wire, wireInName);
                }

                module->fixup_ports();
            }
        }
    }
} DeminoutPass;

PRIVATE_NAMESPACE_END
