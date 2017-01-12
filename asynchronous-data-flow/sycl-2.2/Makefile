
PANDOC=pandoc

HTMLS := $(patsubst %.md,%.html,$(wildcard *.md))

default: html

html: $(HTMLS)
	@echo "HTMLS: $(HTMLS)"

%.html: %.md
	$(PANDOC) -s -o html/$@ $<

.PHONY: clean

clean:
	rm -Rf html/$(HTMLS) 
