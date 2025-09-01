COMPILER  = cc
CFLAGS    = -g -O3 -MMD -MP -Wall -Wextra -Winit-self -Wno-missing-field-initializers
LDFLAGS   = -Wl -s
TARGET    = ./bin/accel-tablet-moded
SRCDIR    = .
SOURCES   = $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/devices/*.c)
OBJDIR    = ./obj
OBJECTS   = $(addprefix $(OBJDIR)/, $(SOURCES:$(SRCDIR)/%.c=%.o))

$(TARGET): $(OBJECTS)
	-mkdir -p $(dir $@)
	$(COMPILER) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	-mkdir -p $(dir $@)
	$(COMPILER) $(CFLAGS) -o $@ -c $<

all: clean $(TARGET)

install: $(TARGET)
	-cp -f $(TARGET) /usr/bin/
	-chmod 755 /usr/bin/$(notdir $(TARGET))
	
dinit: install
	-cp -f services/dinit.service /usr/lib/dinit.d/$(notdir $(TARGET))
	-cp -f services/dinit.conf /etc/default/$(notdir $(TARGET))

clean:
	-rm -f $(OBJECTS) $(TARGET)
