# Design Log — Project 2

## Growth factor and amortized cost

All the conversations were stored in a dynamic array of Message objects. Both _size and capacity_ start at 0 and data_ starts as a nullptr. size_ is used to find the number of messages in the conversation and capacity_ gives us the number of slots in the array.

Once the array becomes full, the function append() creates a bigger array, stores the messages in it and discards the older array. The size of the initial array is 1. Following that, the size of the array doubles from 1, 2, 4, 8 and so on; this means I do not need to create a new array for every single message.

The normal behavior of an append function is adding the message to an available position in the array. The operation that involves creating an array of a larger capacity involves copying all existing messages to the new array, hence the operation takes more time. For instance, for creating the capacity 8, 1 message is copied, then 2 messages are copied and then 4 messages are copied. In n number of append operations, the growth copies will be 1 + 2 + 4 + ... up to less than n; thus, the sum will be less than 2n. Therefore, all n append operations cost O(n) and an amortized cost of one append operation is O(1).
## Rule of Five evidence

Because conversation owns the array at data_, therefore, it deallocates the array through delete[] in its destructor. Deletion of a default conversation that has a null pointer and after the conversation has been moved from is fine.

For the copy constructor and the copy assignment operator, there is a new array that is created and the messages are copied in that array. Only copying of the pointer means that both conversations will have ownership on the same array. Copy assignment ensures self assignment and that the new array is completely built up before deleting the old array. The throwing of message copying means that it deletes the incomplete array and does nothing with the current conversation.

For the move constructor and move assignment operator, the pointer, size, and capacity of the other conversation is taken and the pointer and counts of the current conversation are set to nullptr. Message copying does not happen. Move assignment deletes the array it owns before taking the other array. My experiments showed that copies resulted in different array addresses, while the moves retained the array address of the other.

## Sentinel scanner: bounded `pending_` proof

The stop marker may span multiple chunks. The first chunk could finish with <|end_ while the second one begins with conversation|>. If I output the first chunk immediately, then the user sees part of the marker. pending_ stores such characters up to the point where the scanner can decide whether or not they constitute a complete stop marker.

Let m denote the length of the stop marker. The incomplete stop marker has at most m - 1 characters. The scanner checks pending_ combined with the new chunk and sets keep to the minimum of m - 1 and the length of the text. It stores exactly the last keep characters of the input. It guarantees that pending_.size() is no more than m - 1 after each call to feed().

When the scanner detects the stop marker, then it empties pending_, outputs the text preceding the marker, and notifies that the conversation is done. When there is no stop marker, then flush() emits the characters stored in pending_ as usual text. I checked the bound by inputting 4 MB of text one character at a time.

## What I would change differently

I would add a step where the scanner verifies which suffixes actually form the beginning of the marker. Currently, the scanner holds the last m - 1 characters even when it's obvious that they cannot possibly be used for the marker at all. This works well enough, but regular text must wait until the next chunk is received or flush() is called.

The scanner creates a temporary string out of pending_ and the new chunk as well. However, checking individual characters as they come in might release already safe text earlier, although it would complicate the scanner significantly.
