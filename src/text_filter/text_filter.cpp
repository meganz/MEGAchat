//============================================================================
// Name        : MegaTextFilter.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "text_filter.h"

#include <iostream>
#include <regex>
#include <deque>
#include <algorithm>

#include "text_handler.h"
#include "default_emoticon_handler.h"
#include "default_link_handler.h"
#include "filter_common.h"

using namespace std;

namespace karere {

TextFilter::TextFilter(bool defaults) : borkOnHandlerException(true) {
    if(defaults) {
        setDefaults();
    }
}

void
TextFilter::setBorkOnHandlerException(bool bork) {
    borkOnHandlerException = bork;
}

bool
TextFilter::getBorkOnHandlerException() {
    return borkOnHandlerException;
}

int
TextFilter::registerHandler(SharedHandler &handler) {

    for(auto i : *handler->getFunctionVector()) {
        auto f = handlerMap.find(i);
        if(f != handlerMap.end()) {
            return -1;
        }
        handlerMap.insert({i, handler});
    }

    return 0;
}

int
TextFilter::handleData(std::vector<RenderInfo> &infoVec, const std::string &testStr) {
    std::deque<std::pair<int,std::string>> toBeProcessed = {{ 0, testStr }};

    for(auto &h : handlerMap) {
        std::cout << "testing " << h.first << std::endl;
        if(toBeProcessed.empty()) {
            break;
        }
        std::deque<std::pair<int, std::string>> process;
        for(auto &s : toBeProcessed) {
            process.push_back(s);
        }
        toBeProcessed.clear();

        bool test;
        std::pair<int, std::string> p;

        while(!process.empty()) {
            p = process.front();
            process.pop_front();
            std::smatch match;
            std::regex regex(h.first);

            while((test = std::regex_search(p.second, match, regex))) {
                // Only load the string if the size is greater than 0.
                if(match.position(0) != 0) {
                    toBeProcessed.push_back(
                        {p.first, p.second.substr(0, match.position(0))});
                }

                // Actually perform the processing. We will
                // only process the first match, not interested in
                // subsequent matches.
                RenderInfo info = { h.first, match[0].str(),
                        match.position(0) + p.first };
                h.second->handleData(info);
                infoVec.push_back(info);
                p.first += match.position(0) + match[0].str().length();
                p.second = match.suffix().str();
            }

            // If we have a failure and the length != 0 then push it into the
            // process pile.
            if(!test && p.second.length() != 0) {
                toBeProcessed.push_back(p);
            }
        }
    }

    return 0;

}

void
TextFilter::processData(std::vector<RenderInfo> &infoVec, const std::string &data) {
    std::string dataClone(data);
    std::sort(infoVec.begin(), infoVec.end(), [](const RenderInfo &a, const RenderInfo &b){
        return a.position < b.position;
    });

    for(auto &v : infoVec) {
        std::cout << v.position << std::endl;
    }

    // Process each of the RenderInfo in the vector.
    int diff = 0;
    for(auto &i : infoVec) {
        dataClone.erase(i.position + diff, i.rawData.length());
        dataClone.insert(i.position + diff, i.html);
        diff += i.html.length() - i.rawData.length();
    }

    // Alert all of the listeners.
    for(auto &o : alertVector) {
        o(dataClone);
    }
}

void
TextFilter::addObserver(AlertFunction function) {
    alertVector.push_back(function);
}

void
TextFilter::setDefaults() {
    SharedHandler linkHandler = SharedHandler(new DefaultLinkHandler());
    SharedHandler emoHandler = SharedHandler(new DefaultEmoticonHandler());
    registerHandler(linkHandler);
    registerHandler(emoHandler);
}

} /* namespace karere */
