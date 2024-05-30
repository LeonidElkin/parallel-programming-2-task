#include "gtest/gtest.h"
#include "../src/multiqueue.h"

TEST(Multiqueue, Simple) {
    std::vector<std::size_t> dists = {0, 3, 4, 2, 8, 7};
    std::vector<QueueElement> vertexes(dists.size());
    for (std::size_t i = 0; i < dists.size(); i++) {
        vertexes[i].vertex = i;
    }
    Multiqueue multiqueue(1, 1, dists.size());
    multiqueue.push(&vertexes[0], dists[0]);
    QueueElement * element;
    for (std::size_t i = 0; i < 10; i++) {
        element = multiqueue.pop();
        if (element != &empty_element) {
            break;
        }
    }
    ASSERT_NE(element, &empty_element);
    ASSERT_EQ(dists[0], element->get_dist());
}