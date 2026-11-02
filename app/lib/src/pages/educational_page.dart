// Educational explanations.
//
// The section exists so the drawer reflects the product structure, but no
// material has been written yet. It says so plainly instead of showing a blank
// page or a fake list of articles.
import 'package:flutter/material.dart';

import '../l10n.dart';
import '../theme.dart';
import '../widgets.dart';

class EducationalPage extends StatelessWidget {
  const EducationalPage({super.key, required this.l10n});

  final L10n l10n;

  @override
  Widget build(BuildContext context) {
    final l = l10n;
    final c = context.colors;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SoftCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PageTitle(
                icon: Icons.auto_stories_rounded,
                title: l.t('Educational explanations', 'توضیحات آموزشی'),
                subtitle: l.t(
                  'Written explanations of the ideas behind each module.',
                  'توضیح‌های نوشته‌شده دربارهٔ ایده‌های پشت هر ماژول.',
                ),
              ),
              const SizedBox(height: 20),
              Container(
                width: double.infinity,
                padding: const EdgeInsets.symmetric(
                  horizontal: 18,
                  vertical: 34,
                ),
                decoration: BoxDecoration(
                  color: c.cardAlt,
                  borderRadius: BorderRadius.circular(18),
                  border: Border.all(color: c.line),
                ),
                child: Column(
                  children: [
                    Container(
                      width: 46,
                      height: 46,
                      decoration: BoxDecoration(
                        color: c.accentSoft,
                        shape: BoxShape.circle,
                        border: Border.all(
                          color: c.accent.withValues(alpha: 0.25),
                        ),
                      ),
                      child: Icon(
                        Icons.menu_book_outlined,
                        size: 22,
                        color: c.accent,
                      ),
                    ),
                    const SizedBox(height: 14),
                    Text(
                      l.t(
                        'Nothing has been written here yet.',
                        'هنوز مطلبی اینجا نوشته نشده است.',
                      ),
                      textAlign: TextAlign.center,
                      style: Theme.of(
                        context,
                      ).textTheme.titleMedium?.copyWith(fontSize: 14.5),
                    ),
                    const SizedBox(height: 6),
                    ConstrainedBox(
                      constraints: const BoxConstraints(maxWidth: 460),
                      child: Text(
                        l.t(
                          'This section is reserved for the written explanations. '
                              'Until they are added, every module already carries its own '
                              'notes next to the input and the result.',
                          'این بخش برای توضیحات نوشته‌شده در نظر گرفته شده است. '
                              'تا زمانی که اضافه شوند، هر ماژول توضیح‌های خودش را '
                              'کنار ورودی و نتیجه دارد.',
                        ),
                        textAlign: TextAlign.center,
                        style: Theme.of(
                          context,
                        ).textTheme.bodySmall?.copyWith(height: 1.8),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
