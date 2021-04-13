#include "neural/loader.h"
#include "neural/network_basic.h"
#include "utils/parser.h"
#include "utils/log.h"

#include "iostream"
#include "sstream"

#ifdef USE_FAST_PARSER
#include "fast_float.h"
#endif 


DNNLoder& Get() {
    static DNNLoder lodaer;
    return lodaer;
}

void DNNLoder::FormFile(std::shared_ptr<DNNWeights> weights, std::string filename) const {
    auto file = std::ifstream{};
    auto buffer = std::stringstream{};
    auto line = std::string{};

    file.open(filename.c_str());

    if (!file.is_open()) {
        ERROR << "Couldn't open file:" << ' ' << filename << '!' << std::endl;
        return;
    }

    while(std::getline(file, line)) {
        if (line.empty()) {
            // Remove the unused space.
            continue;
        }
        buffer << line << std::endl;
    }
    file.close();
    
    try {
        Parse(weights, buffer);
    } catch (const char* err) {
        // Should be not happned.
        ERROR << "Loading network file fail!" << std::endl
                 << "    Cause:" << ' ' << err << '.' << std::endl;
    }
}

void DNNLoder::Parse(std::shared_ptr<DNNWeights> weights, std::istream &buffer) const {
   /**
    * get main
    * get info
    *   (Aboaut the network information.)
    * end
    *
    * get struct
    *   (Aboaut the network struct.)
    * end
    *
    * get parameters
    *   (The network weights are here. The struct follow
    *    the struct scroll)
    * end
    * end
    *
    */
    auto counter = size_t{0};
    auto line = std::string{};

    if (std::getline(buffer, line)) {
        const auto p = CommandParser(line, 2);
        if (p.GetCommand(0)->Get<std::string>() != "get" ||
                p.GetCommand(1)->Get<std::string>() != "main") {
            throw "Weights file format is not acceptable";
        } else {
            counter++;
        }
    } else {
        throw "Weights file is empty";
    }

    auto netinfo = NetInfo{};
    auto netstruct = NetStruct{};

    while (std::getline(buffer, line)) {
        const auto p = CommandParser(line, 2);
        if (p.GetCommand(0)->Get<std::string>() == "get") {
            if (p.GetCommand(1)->Get<std::string>() == "info") {
                ParseInfo(netinfo, buffer);
            } else if (p.GetCommand(1)->Get<std::string>() == "struct") {
                ParseStruct(netstruct, buffer);
            } else {
                counter++;
            }
        } else if (p.GetCommand(0)->Get<std::string>() == "end") {
            counter--;
        }
    }

    buffer.clear();
    buffer.seekg(0, std::ios::beg);

    // Now start to parse the weights.
    while (std::getline(buffer, line)) {
        const auto p = CommandParser(line);
        if (p.GetCommand(0)->Get<std::string>() == "get") {
            if (p.GetCommand(1)->Get<std::string>() == "parameters") {

            }
        }
    }
}

void DNNLoder::ParseInfo(NetInfo &netinfo, std::istream &buffer) const {
    auto line = std::string{};
    while (std::getline(buffer, line)) {
        const auto p = CommandParser(line);
        if (p.GetCommand(0)->Get<std::string>()[0] == '#') {
            continue;
        } else if (p.GetCommand(0)->Get<std::string>() == "end") {
            break;
        }
        netinfo.emplace(p.GetCommand(0)->Get<std::string>(),
                            p.GetCommand(1)->Get<std::string>());
    }


    const auto NotFound = [](NetInfo &netinfo, std::string target) -> bool {
        return std::end(netinfo) == netinfo.find(target);
    };

    if (NotFound(netinfo, "InputChannels")) {
        throw "InputChannels must be provided";
    }
    if (NotFound(netinfo, "ResidualBlocks")) {
        throw "ResidualBlocks must be provided";
    }
    if (NotFound(netinfo, "ResidualChannels")) {
        throw "ResidualChannels must be provided";
    }
    if (NotFound(netinfo, "PolicyExtract")) {
        throw "PolicyExtract must be provided";
    }
    if (NotFound(netinfo, "ValueExtract")) {
        throw "ValueExtract must be provided";
    }
}

void DNNLoder::ParseStruct(NetStruct &netstruct, std::istream &buffer) const {
    auto line = std::string{};
    auto cnt = size_t{0};
    while (std::getline(buffer, line)) {
        const auto p = CommandParser(line);
        if (p.GetCommand(0)->Get<std::string>()[0] == '#') {
            continue;
        } else if (p.GetCommand(0)->Get<std::string>() == "end") {
            break;
        }
            
        netstruct.emplace_back(LayerShape{});
        for (auto i = size_t{1}; i < p.GetCount(); ++i) {
            const auto s = p.GetCommand(i)->Get<int>();
            netstruct[cnt].emplace_back(s);
        }

        if (p.GetCommand(0)->Get<std::string>() == "FullyConnect") {
            if (netstruct[cnt].size() != 2) {
                throw "The FullyConnect Layer shape is error";
            }
        } else if (p.GetCommand(0)->Get<std::string>() == "Convolve") {
            if (netstruct[cnt].size() != 3) {
                throw "The Convolve Layer shape is error";
            }
        } else if (p.GetCommand(0)->Get<std::string>() == "BatchNorm") {
            if (netstruct[cnt].size() != 1) {
                throw "The BatchNorm layer shape is error";
            }
        } else {
            throw "The layer shape is error";
        }
        cnt++;
    }
}

void DNNLoder::FillWeights(NetInfo &netinfo,
                           NetStruct &netstruct,
                           std::shared_ptr<DNNWeights> weights,
                           std::istream &buffer) const {
    weights->input_channels = std::stoi(netinfo["InputChannels"]);

    weights->residual_blocks = std::stoi(netinfo["ResidualBlocks"]);
    weights->residual_channels = std::stoi(netinfo["ResidualChannels"]);

    weights->policy_extract_channels = std::stoi(netinfo["PolicyExtract"]);
    weights->value_extract_channels = std::stoi(netinfo["ValueExtract"]);


    if (weights->input_channels != kInputChannels) {
        throw "The number of input channels is wrong.";
    }

    // input layer
    const auto inputs_cnt = 2;
    const auto input_conv_shape = netstruct[0];
    FillConvolutionLayer(weights->input_conv,
                         buffer,
                         input_conv_shape[0],
                         input_conv_shape[1],
                         input_conv_shape[2]);
        
    const auto input_bn_shape = netstruct[1];
    FillBatchnormLayer(weights->input_bn,
                       buffer,
                       input_bn_shape[0]);

    if (weights->residual_channels != input_conv_shape[1] ||
            weights->residual_channels != input_bn_shape[0] ||
            input_conv_shape[2] != 3) {
        throw "The input layers are wrong";
    }

    const auto residuals = weights->residual_blocks;
    auto se_cnt = 0;
    for (int b = 0; b < residuals; ++b) {
        const auto t_offset = 4 * b + 2 * se_cnt + inputs_cnt;
        const auto res_conv1_shape = netstruct[t_offset];
        const auto res_bn1_shape = netstruct[t_offset+1];
        const auto res_conv2_shape = netstruct[t_offset+2];
        const auto res_bn2_shape = netstruct[t_offset+3];

        weights->tower.emplace_back(ResidualBlock{});
        auto tower_ptr = weights->tower.data() + b;
        
        FillConvolutionLayer(tower_ptr->conv1,
                             buffer,
                             res_conv1_shape[0],
                             res_conv1_shape[1],
                             res_conv1_shape[2]);

        FillBatchnormLayer(tower_ptr->bn1,
                           buffer,
                           res_bn1_shape[0]);

        if (weights->residual_channels != res_conv1_shape[0] ||
                weights->residual_channels != res_conv1_shape[1] ||
                weights->residual_channels != res_bn1_shape[0] || 
                res_conv1_shape[2] != 3) {
            throw "The Residual Block(1) is wrong";
        }

        FillConvolutionLayer(tower_ptr->conv2,
                             buffer,
                             res_conv2_shape[0],
                             res_conv2_shape[1],
                             res_conv2_shape[2]);

        FillBatchnormLayer(tower_ptr->bn2,
                           buffer,
                           res_bn2_shape[0]);

        if (weights->residual_channels != res_conv2_shape[0] ||
                weights->residual_channels != res_conv2_shape[1] ||
                weights->residual_channels != res_bn2_shape[0] ||
                res_conv2_shape[2] != 3) {
            throw "The Residual Block(2) is wrong";
        }
            
        const auto res_next_shape = netstruct[t_offset+4];
            
        if (res_next_shape.size() == 2 /* fullyconnect layer */) {
            tower_ptr->apply_se = true;
            se_cnt++;
            const auto se_extend_shape = netstruct[t_offset+4];
            const auto se_squeeze_shape = netstruct[t_offset+5];
            FillFullyconnectLayer(tower_ptr->extend,
                                  buffer,
                                  se_extend_shape[0],
                                  se_extend_shape[1]);
            FillFullyconnectLayer(tower_ptr->squeeze,
                                  buffer,
                                  se_squeeze_shape[0],
                                  se_squeeze_shape[1]);

            if (se_extend_shape[1] != se_squeeze_shape[0]) {
                throw "The SE Unit size is wrong.";
            }
            if (2 * se_extend_shape[0] != se_squeeze_shape[1] ||
                se_extend_shape[0] != weights->residual_channels) {
                throw "The SE Unit size is wrong.";
            }
                
            tower_ptr->se_size = se_extend_shape[1];
        } else {
            tower_ptr->apply_se = false;
        }
    }

    // policy head
    // value head

    ProcessWeights(weights, false);
}

void DNNLoder::ProcessWeights(std::shared_ptr<DNNWeights> &weights, bool winograd) const {

}

void DNNLoder::GetWeightsFromBuffer(std::vector<float> &weights, std::istream &buffer) const {
    weights.clear();
    auto line = std::string{};

    if (std::getline(buffer, line)) {
        // On MacOS, if the numeric is too small, stringstream
        // can not parse the number to float, but double is ok.
        double weight;

#ifdef USE_FAST_PARSER
        auto start_ptr = line.data();
        auto end_ptr = line.data();
        auto line_size = line.size();
        auto finish_ptr = line.data() + line_size;
        weights.reserve(line_size / 12);

        while (*end_ptr == ' ') {
            end_ptr++;
            if (end_ptr == finish_ptr) break;
        }
        start_ptr = end_ptr;

        while (start_ptr != finish_ptr) {
            while (*end_ptr != ' ') {
                end_ptr++;
                if (end_ptr == finish_ptr) break;
            }
            const auto is_ok = fast_float::from_chars<double>(start_ptr, end_ptr, weight);
            if (is_ok.ec != std::errc()) {
                throw "There is non-numeric in parameters";
            }

            weights.emplace_back(weight);

            while (*end_ptr == ' ') {
                end_ptr++;
                if (end_ptr == finish_ptr) break;
            }
            start_ptr = end_ptr;
        }
#else 
        std::stringstream line_buffer(line);
        while(line_buffer >> weight) {
            weights.emplace_back(weight);
        }
#endif
    }
}

void DNNLoder::FillFullyconnectLayer(LinearLayer &layer,
                                     std::istream &buffer,
                                     const int in_size,
                                     const int out_size) const {
    auto weights = std::vector<float>{};
    layer.Set(in_size, out_size);    

    GetWeightsFromBuffer(weights, buffer);
    layer.LoadWeights(weights);

    GetWeightsFromBuffer(weights, buffer);
    layer.LoadBiases(weights);
}

void DNNLoder::FillBatchnormLayer(BatchNormLayer &layer,
                                  std::istream &buffer,
                                  const int channels) const {
    auto weights = std::vector<float>{};
    layer.Set(channels);

    GetWeightsFromBuffer(weights, buffer);
    layer.LoadMeans(weights);

    GetWeightsFromBuffer(weights, buffer);
    layer.LoadStddevs(weights);
}

void DNNLoder::FillConvolutionLayer(ConvLayer &layer,
                                    std::istream &buffer,
                                    const int in_channels,
                                    const int out_channels,
                                    const int kernel_size) const {
    auto weights = std::vector<float>{};    
    layer.Set(in_channels, out_channels, kernel_size);

    GetWeightsFromBuffer(weights, buffer);
    layer.LoadWeights(weights);
    
    GetWeightsFromBuffer(weights, buffer);
    layer.LoadBiases(weights);
}

