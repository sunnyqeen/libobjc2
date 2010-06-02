#include <stdio.h>
#include "objc/runtime.h"
#include "loader.h"

#define BUFFER_TYPE struct objc_category
#include "buffer.h"

static void register_methods(struct objc_class *cls, struct objc_method_list *l)
{
	if (NULL == l) { return; }

	// Replace the method names with selectors.
	objc_register_selectors_from_list(l);
	// Add the method list at the head of the list of lists.
	l->next = cls->methods;
	cls->methods = l;
	// Update the dtable to catch the new methods.
	// FIXME: We can make this more efficient by simply passing the new method
	// list to the dtable and telling it only to update those methods.
	objc_update_dtable_for_class(cls);
}

static void load_category(struct objc_category *cat, struct objc_class *class)
{
	register_methods(class, cat->instance_methods);
	register_methods(class->isa, cat->class_methods);

	if (cat->protocols)
	{
		objc_init_protocols(cat->protocols);
		cat->protocols->next = class->protocols;
		class->protocols = cat->protocols;
	}
}

static BOOL try_load_category(struct objc_category *cat)
{
	Class class = (Class)objc_getClass(cat->class_name);
	if (Nil != class)
	{
		load_category(cat, class);
		return YES;
	}
	return NO;
}

/**
 * Attaches a category to its class, if the class is already loaded.  Buffers
 * it for future resolution if not.
 */
void objc_try_load_category(struct objc_category *cat)
{
	if (!try_load_category(cat))
	{
		set_buffered_object_at_index(cat, buffered_objects++);
	}
}

void objc_load_buffered_categories(void)
{
	BOOL shouldReshuffle = NO;

	for (unsigned i=0 ; i<buffered_objects ; i++)
	{
		struct objc_category *c = buffered_object_at_index(i);
		if (NULL != c)
		{
			if (try_load_category(c))
			{
				set_buffered_object_at_index(NULL, i);
				shouldReshuffle = YES;
			}
		}
	}

	if (shouldReshuffle)
	{
		compact_buffer();
	}
}

