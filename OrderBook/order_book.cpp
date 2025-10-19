#include "order_book.h"
#include <iostream>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <chrono>
#include <iomanip>

namespace {
    struct OrderBookData {
        std::unordered_map<uint64_t, Order> orders;  
        std::map<double, uint64_t, std::greater<double>> bids; 
        std::map<double, uint64_t> asks;  
        std::map<double, uint64_t> bid_order_counts;  
        std::map<double, uint64_t> ask_order_counts;  


        mutable uint64_t color_cycle = 0;
    };

    uint64_t get_current_timestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    // ANSI color codes for terminal
    class Color {
    public:
           public:
        static const std::string RESET;
        static const std::string BOLD;
        static const std::string WHITE;
        
        static const std::string RED;
        static const std::string GREEN;
       
        
        static const std::string RED_BG;
        static const std::string GREEN_BG;
    };

    const std::string Color::RESET = "\033[0m";
    const std::string Color::BOLD = "\033[1m";
    const std::string Color::WHITE = "\033[97m";
    
    const std::string Color::RED = "\033[31m";
    const std::string Color::GREEN = "\033[32m";
    
    const std::string Color::RED_BG = "\033[41m";
    const std::string Color::GREEN_BG = "\033[42m";


        std::string get_bid_color(uint64_t cycle) {
        static const std::string colors[] = {Color::WHITE};
        return Color::WHITE;
    }

    std::string get_ask_color(uint64_t cycle) {
        static const std::string colors[] = {Color::WHITE};
        return Color::WHITE;
    }

    std::string get_bid_bg_color(uint64_t cycle) {
        static const std::string colors[] = {Color::GREEN_BG, "\033[48;5;28m", "\033[48;5;34m"};
        return colors[cycle % 3];
    }

    std::string get_ask_bg_color(uint64_t cycle) {
        static const std::string colors[] = {Color::RED_BG, "\033[48;5;88m", "\033[48;5;124m"};
        return colors[cycle % 3];
    }


}

class OrderBook::OrderBookImpl {
private:
    OrderBookData data;
    uint64_t next_order_id = 1000; 
    uint64_t max_bid_quantity = 0;
    uint64_t max_ask_quantity = 0;

public:
    std::vector<Trade> add_order(const Order& order) {
        std::vector<Trade> trades;
        
        if (data.orders.find(order.order_id) != data.orders.end()) {
            return trades; 
        }

        Order new_order = order;
        
        if (new_order.order_type == OrderType::MARKET) {
            if (new_order.is_buy) {
                while (new_order.quantity > 0 && !data.asks.empty()) {
                    auto best_ask = data.asks.begin();
                    match_orders(new_order, best_ask->first, trades);
                }
            } else {
                while (new_order.quantity > 0 && !data.bids.empty()) {
                    auto best_bid = data.bids.begin();
                    match_orders(new_order, best_bid->first, trades);
                }
            }
        } else {
            if (new_order.is_buy) {
                while (new_order.quantity > 0 && !data.asks.empty()) {
                    auto best_ask = data.asks.begin();
                    if (best_ask->first <= new_order.price) {
                        match_orders(new_order, best_ask->first, trades);
                    } else {
                        break;
                    }
                }
                
                if (new_order.quantity > 0) {
                    data.orders[new_order.order_id] = new_order;
                    data.bids[new_order.price] += new_order.quantity;
                    data.bid_order_counts[new_order.price]++;
                    update_max_quantities();
                }
            } else {
                while (new_order.quantity > 0 && !data.bids.empty()) {
                    auto best_bid = data.bids.begin();
                    if (best_bid->first >= new_order.price) {
                        match_orders(new_order, best_bid->first, trades);
                    } else {
                        break;
                    }
                }
                
                if (new_order.quantity > 0) {
                    data.orders[new_order.order_id] = new_order;
                    data.asks[new_order.price] += new_order.quantity;
                    data.ask_order_counts[new_order.price]++;
                    update_max_quantities();
                }
            }
        }
        
        return trades;
    }

    bool cancel_order(uint64_t order_id) {
        auto it = data.orders.find(order_id);
        if (it == data.orders.end()) {
            return false;
        }

        const Order& order = it->second;
        
        if (order.order_type == OrderType::LIMIT) {
            if (order.is_buy) {
                auto bid_it = data.bids.find(order.price);
                if (bid_it != data.bids.end()) {
                    if (bid_it->second <= order.quantity) {
                        data.bids.erase(bid_it);
                        data.bid_order_counts.erase(order.price);
                    } else {
                        bid_it->second -= order.quantity;
                        data.bid_order_counts[order.price]--;
                    }
                }
            } else {
                auto ask_it = data.asks.find(order.price);
                if (ask_it != data.asks.end()) {
                    if (ask_it->second <= order.quantity) {
                        data.asks.erase(ask_it);
                        data.ask_order_counts.erase(order.price);
                    } else {
                        ask_it->second -= order.quantity;
                        data.ask_order_counts[order.price]--;
                    }
                }
            }
            update_max_quantities();
        }

        data.orders.erase(it);
        return true;
    }

    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
        auto it = data.orders.find(order_id);
        if (it == data.orders.end()) {
            return false;
        }

        Order& order = it->second;
        
        if (order.order_type == OrderType::LIMIT) {
            if (order.is_buy) {
                auto bid_it = data.bids.find(order.price);
                if (bid_it != data.bids.end()) {
                    if (bid_it->second <= order.quantity) {
                        data.bids.erase(bid_it);
                        data.bid_order_counts.erase(order.price);
                    } else {
                        bid_it->second -= order.quantity;
                        data.bid_order_counts[order.price]--;
                    }
                }
            } else {
                auto ask_it = data.asks.find(order.price);
                if (ask_it != data.asks.end()) {
                    if (ask_it->second <= order.quantity) {
                        data.asks.erase(ask_it);
                        data.ask_order_counts.erase(order.price);
                    } else {
                        ask_it->second -= order.quantity;
                        data.ask_order_counts[order.price]--;
                    }
                }
            }

            order.price = new_price;
            order.quantity = new_quantity;

            if (order.is_buy) {
                data.bids[new_price] += new_quantity;
                data.bid_order_counts[new_price]++;
            } else {
                data.asks[new_price] += new_quantity;
                data.ask_order_counts[new_price]++;
            }
            
            update_max_quantities();
        }

        return true;
    }

    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids_out, std::vector<PriceLevel>& asks_out) const {
        bids_out.clear();
        asks_out.clear();

        size_t count = 0;
        for (auto it = data.bids.begin(); it != data.bids.end() && count < depth; ++it, ++count) {
            bids_out.push_back({it->first, it->second, data.bid_order_counts.at(it->first)});
        }

        count = 0;
        for (auto it = data.asks.begin(); it != data.asks.end() && count < depth; ++it, ++count) {
            asks_out.push_back({it->first, it->second, data.ask_order_counts.at(it->first)});
        }
    }

      void print_book(size_t depth) const {
        std::vector<PriceLevel> bids, asks;
        get_snapshot(depth, bids, asks);

        data.color_cycle = (data.color_cycle + 1) % 3;

        std::cout << Color::BOLD << Color::WHITE 
                  << "\n================================ ORDER BOOK (Depth: " << depth << ") ================================" 
                  << Color::RESET << "\n";
        
        // ASKS SECTION
        std::cout << Color::BOLD << "\n--------------------------------- ASKS (SELL) ---------------------------------" << Color::RESET << "\n";
        std::cout << Color::BOLD << "   PRICE       QUANTITY      ORDERS     DEPTH %           DEPTH CHART" << Color::RESET << "\n";
        std::cout << Color::BOLD << "------------------------------------------------------------------------" << Color::RESET << "\n";
        
        if (asks.empty()) {
            std::cout << "                        No asks in the book\n";
        } else {
            for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
                double percentage = max_ask_quantity > 0 ? (double)it->total_quantity / max_ask_quantity : 0;
                int bars = static_cast<int>(percentage * 30); // 30 chars max for bars
                
                std::string color = get_ask_color(data.color_cycle);
                std::string bg_color = get_ask_bg_color(data.color_cycle);
                
                std::cout << "  " << color << std::setw(8) << std::fixed << std::setprecision(2) << it->price << Color::RESET
                          << "    " << color << std::setw(10) << it->total_quantity << Color::RESET
                          << "      " << color << std::setw(4) << it->order_count << Color::RESET
                          << "      " << color << std::setw(5) << std::setprecision(1) << (percentage * 100) << "%" << Color::RESET
                          << "      ";
                
                // Visual depth bars
                std::cout << bg_color << Color::WHITE;
                for (int i = 0; i < bars; ++i) {
                    std::cout << "|";
                }
                std::cout << Color::RESET;
                
                // Fill remaining space
                for (int i = bars; i < 30; ++i) {
                    std::cout << " ";
                }
                std::cout << "\n";
            }
        }

        // SPREAD INFORMATION
        double best_bid = get_best_bid();
        double best_ask = get_best_ask();
        if (best_bid > 0 && best_ask > 0) {
            double spread = best_ask - best_bid;
            double spread_percent = (spread / best_bid) * 100;
            
            std::cout << Color::BOLD << "\n------------------------------- SPREAD INFORMATION ------------------------------" << Color::RESET << "\n";
            std::cout << "  Best Bid: " << get_bid_color(data.color_cycle) << std::setw(10) << std::fixed << std::setprecision(2) << best_bid << Color::RESET
                      << "  Best Ask: " << get_ask_color(data.color_cycle) << std::setw(10) << best_ask << Color::RESET
                      << "  Spread: " << Color::WHITE << std::setw(8) << spread << " (" << std::setprecision(2) << spread_percent << "%)" << Color::RESET << "\n";
            std::cout << Color::BOLD << "------------------------------------------------------------------------" << Color::RESET << "\n";
        }

        // BIDS SECTION
        std::cout << Color::BOLD << "\n--------------------------------- BIDS (BUY) ----------------------------------" << Color::RESET << "\n";
        std::cout << Color::BOLD << "   PRICE       QUANTITY      ORDERS     DEPTH %           DEPTH CHART" << Color::RESET << "\n";
        std::cout << Color::BOLD << "------------------------------------------------------------------------" << Color::RESET << "\n";
        
        if (bids.empty()) {
            std::cout << "                        No bids in the book\n";
        } else {
            for (const auto& level : bids) {
                double percentage = max_bid_quantity > 0 ? (double)level.total_quantity / max_bid_quantity : 0;
                int bars = static_cast<int>(percentage * 30);
                
                std::string color = get_bid_color(data.color_cycle);
                std::string bg_color = get_bid_bg_color(data.color_cycle);
                
                std::cout << "  " << color << std::setw(8) << std::fixed << std::setprecision(2) << level.price << Color::RESET
                          << "    " << color << std::setw(10) << level.total_quantity << Color::RESET
                          << "      " << color << std::setw(4) << level.order_count << Color::RESET
                          << "      " << color << std::setw(5) << std::setprecision(1) << (percentage * 100) << "%" << Color::RESET
                          << "      ";
                
                // Visual depth bars
                std::cout << bg_color << Color::WHITE;
                for (int i = 0; i < bars; ++i) {
                    std::cout << "|";
                }
                std::cout << Color::RESET;
                
                // Fill remaining space
                for (int i = bars; i < 30; ++i) {
                    std::cout << " ";
                }
                std::cout << "\n";
            }
        }

        // LEGEND
        std::cout << Color::BOLD << "\n----------------------------------- LEGEND ------------------------------------" << Color::RESET << "\n";
        std::cout << "  " << get_ask_color(data.color_cycle) << "PRICE" << Color::RESET 
                  << "     - Trading price level\n";
        std::cout << "  " << get_ask_color(data.color_cycle) << "QUANTITY" << Color::RESET 
                  << "  - Total units available\n";
        std::cout << "  " << get_ask_color(data.color_cycle) << "ORDERS" << Color::RESET 
                  << "    - Number of individual orders\n";
        std::cout << "  " << get_ask_color(data.color_cycle) << "DEPTH %" << Color::RESET 
                  << "   - Relative to maximum quantity\n";
        std::cout << "  " << get_bid_color(data.color_cycle) << "DEPTH CHART" << Color::RESET 
                  << " - Visual quantity representation (30 chars)\n";
        std::cout << Color::BOLD << "================================================================================" << Color::RESET << "\n\n";
    }

    
    double get_best_bid() const {
        if (data.bids.empty()) return 0.0;
        return data.bids.begin()->first;
    }

    double get_best_ask() const {
        if (data.asks.empty()) return 0.0;
        return data.asks.begin()->first;
    }

    bool order_exists(uint64_t order_id) const {
        return data.orders.find(order_id) != data.orders.end();
    }

    void get_price_levels(std::vector<PriceLevel>& bids_out, std::vector<PriceLevel>& asks_out) const {
        get_snapshot(1000, bids_out, asks_out);
    }

private:
    void update_max_quantities() {
        max_bid_quantity = 0;
        for (const auto& bid : data.bids) {
            if (bid.second > max_bid_quantity) max_bid_quantity = bid.second;
        }
        
        max_ask_quantity = 0;
        for (const auto& ask : data.asks) {
            if (ask.second > max_ask_quantity) max_ask_quantity = ask.second;
        }
    }

    void match_orders(Order& incoming_order, double match_price, std::vector<Trade>& trades) {
        std::vector<uint64_t> orders_to_remove;
        
        if (incoming_order.is_buy) {
            for (auto& order_pair : data.orders) {
                if (!order_pair.second.is_buy && 
                    order_pair.second.order_type == OrderType::LIMIT &&
                    order_pair.second.price == match_price && 
                    incoming_order.quantity > 0) {
                    
                    uint64_t trade_quantity = std::min(incoming_order.quantity, order_pair.second.quantity);
                    
                    Trade trade;
                    trade.buy_order_id = incoming_order.order_id;
                    trade.sell_order_id = order_pair.first;
                    trade.price = match_price;
                    trade.quantity = trade_quantity;
                    trade.timestamp_ns = get_current_timestamp();
                    trades.push_back(trade);
                    
                    incoming_order.quantity -= trade_quantity;
                    order_pair.second.quantity -= trade_quantity;
                    
                    data.asks[match_price] -= trade_quantity;
                    if (data.asks[match_price] == 0) {
                        data.asks.erase(match_price);
                        data.ask_order_counts.erase(match_price);
                    }
                    
                    if (order_pair.second.quantity == 0) {
                        orders_to_remove.push_back(order_pair.first);
                    }
                    
                    if (incoming_order.quantity == 0) break;
                }
            }
        } else {
            for (auto& order_pair : data.orders) {
                if (order_pair.second.is_buy && 
                    order_pair.second.order_type == OrderType::LIMIT &&
                    order_pair.second.price == match_price && 
                    incoming_order.quantity > 0) {
                    
                    uint64_t trade_quantity = std::min(incoming_order.quantity, order_pair.second.quantity);
                    
                    Trade trade;
                    trade.buy_order_id = order_pair.first;
                    trade.sell_order_id = incoming_order.order_id;
                    trade.price = match_price;
                    trade.quantity = trade_quantity;
                    trade.timestamp_ns = get_current_timestamp();
                    trades.push_back(trade);
                    
                    incoming_order.quantity -= trade_quantity;
                    order_pair.second.quantity -= trade_quantity;
                    
                    data.bids[match_price] -= trade_quantity;
                    if (data.bids[match_price] == 0) {
                        data.bids.erase(match_price);
                        data.bid_order_counts.erase(match_price);
                    }
                    
                    if (order_pair.second.quantity == 0) {
                        orders_to_remove.push_back(order_pair.first);
                    }
                    
                    if (incoming_order.quantity == 0) break;
                }
            }
        }
        
        for (uint64_t order_id : orders_to_remove) {
            data.orders.erase(order_id);
        }
        
        update_max_quantities();
    }
};

OrderBook::OrderBook() : impl(new OrderBookImpl()) {}
OrderBook::~OrderBook() { delete impl; }
std::vector<Trade> OrderBook::add_order(const Order& order) { return impl->add_order(order); }
bool OrderBook::cancel_order(uint64_t order_id) { return impl->cancel_order(order_id); }
bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) { return impl->amend_order(order_id, new_price, new_quantity); }
void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const { impl->get_snapshot(depth, bids, asks); }
void OrderBook::print_book(size_t depth) const { impl->print_book(depth); }
double OrderBook::get_best_bid() const { return impl->get_best_bid(); }
double OrderBook::get_best_ask() const { return impl->get_best_ask(); }
bool OrderBook::order_exists(uint64_t order_id) const { return impl->order_exists(order_id); }
void OrderBook::get_price_levels(std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const { impl->get_price_levels(bids, asks); }